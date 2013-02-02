#include <node.h>
#include <string>
#include <iostream>
#include <fstream>

using namespace v8;
using namespace std;

// Forward declaration. Usually, you do this in a header file.
Handle<Value> Async(const Arguments& args);
void AsyncWork(uv_work_t* req);
void AsyncAfter(uv_work_t* req);


//
// We use the Gregory–Leibniz series to compute PI
// see http://en.wikipedia.org/wiki/Pi#Rapidly_convergent_series
long double gregory_leibniz(unsigned long long from, unsigned long long to){
  long double sum = 0.0;
  float sign	=1.0;
  for (unsigned long long step=from;step<to;step+=2){
    sum+=4.0/(step)*sign;
    sign*=-1.0;
  }
  return sum;	
}

//
// We use a struct to store information about the asynchronous "work request".
struct JS_Context {
    Persistent<Function> callback;

    bool error;
    std::string error_message;

    // Custom data you can pass through.
    unsigned long long from, to;    
    double result;
};

//
// This is the function called directly from JavaScript land. 
Handle<Value> Async(const Arguments& args) {
    HandleScope scope;

    if (!args[2]->IsFunction()) {
        return ThrowException(Exception::TypeError(
            String::New("Second argument must be a callback function")));
    }
    

    // Our fake API allows the user to tell us whether we should cause an error.
    unsigned long long from = args[0]->IntegerValue();
    unsigned long long to = args[1]->IntegerValue();
    
    //
    // basic trick to manage big number
    from=(from>=1000000)?from*1000:from;
    to=(to>=1000000)?to*1000:to;
  	//DEBUG: cout << "from: " << from << " to: " << to << endl;
    
    
    //
    // get callback function
    Local<Function> callback = Local<Function>::Cast(args[2]);


    //
    // create  worker
    JS_Context* context = new JS_Context();
    context->error = false;
    context->from=from;
    context->to=to;
    context->callback = Persistent<Function>::New(callback);

    // This creates the work request struct.
    uv_work_t *req = new uv_work_t();
    req->data = context;

    // Schedule our work request with libuv. Here you can specify the functions
    // that should be executed in the threadpool and back in the main thread
    // after the threadpool function completed.
    int status = uv_queue_work(uv_default_loop(), req, AsyncWork, AsyncAfter);
    assert(status == 0);

    return Undefined();
}



//
// this is the running worker
void AsyncWork(uv_work_t* req) {
    JS_Context* context = static_cast<JS_Context*>(req->data);

    // Do work in threadpool here.
    context->result = gregory_leibniz(context->from+1,context->to);

    // If the work we do fails, set context->error_message to the error string
    // and context->error to true.
}

// This function is executed in the main V8/JavaScript thread. That means it's
// safe to use V8 functions again. Don't forget the HandleScope!
void AsyncAfter(uv_work_t* req) {
    HandleScope scope;
    JS_Context* context = static_cast<JS_Context*>(req->data);

    //
    // on error
    if (context->error) {
        Local<Value> err = Exception::Error(String::New(context->error_message.c_str()));

        // Prepare the parameters for the callback function.
        const unsigned argc = 1;
        Local<Value> argv[argc] = { err };

        TryCatch try_catch;
        context->callback->Call(Context::GetCurrent()->Global(), argc, argv);
        if (try_catch.HasCaught()) {
            node::FatalException(try_catch);
        }        
    } else {
        const unsigned argc = 2;
        Local<Value> argv[argc] = {
            Local<Value>::New(Null()),
            Local<Value>::New(Number::New(context->result))
        };

        TryCatch try_catch;
        context->callback->Call(Context::GetCurrent()->Global(), argc, argv);
        if (try_catch.HasCaught()) {
            node::FatalException(try_catch);
        }
    }

    // The callback is a permanent handle, so we have to dispose of it manually.
    context->callback.Dispose();

    // We also created the context and the work_req struct with new, so we have to
    // manually delete both.
    delete context;
    delete req;
}

void RegisterModule(Handle<Object> target) {
    target->Set(String::NewSymbol("compute"),
        FunctionTemplate::New(Async)->GetFunction());
}

NODE_MODULE(PI, RegisterModule);
