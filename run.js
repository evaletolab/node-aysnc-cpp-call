// issue: native big number in node,
// https://github.com/joyent/node/issues/1784

var PI = require('./build/Release/PI');
var total=0.0;

//
// define computation range for workers
var workers=[
  [0,1000000],
  [1000000,2000000],
  [2000000,3000000],
  [3000000,4000000],
  [4000000,4000000]
];

require('async').forEach(workers,function(index,cb){
  
  //
  // run native code in separate thread
  var id=PI.compute(index[0],index[1], function(err, sum) {
      total+=sum;
      cb(err);
  });
  
  console.log("worker [",id,"] computing:" +index);
},
function(err){

  //
  // all workers are done!
  var _PI="3.14159265358979323846264338327950288419716939937510582097494459230781640628620899";

  console.log("PI target: ",_PI);
  console.log("Final Sum: ",total);
});
