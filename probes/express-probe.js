/*******************************************************************************
 * Copyright 2015 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*******************************************************************************/
var Probe = require('../lib/probe.js');
var aspect = require('../lib/aspect.js');
var request = require('../lib/request.js');
var util = require('util');
var am = require('../');

function ExpressProbe() {
  Probe.call(this, 'express');
}

util.inherits(ExpressProbe, Probe);

//This method attaches our probe to the instance of the express module (target)
ExpressProbe.prototype.attach = function(name, target) {

  var that = this;
  if( name != "express" ) return target;
  if( target.__ddProbeAttached__ ) return target;
  target.__ddProbeAttached__ = true;

  var data = {};

  var applicationMethods = ['checkout', 'copy', 'delete', 'get', 'head', 'lock', 'merge', 'mkactivity',
                            'mkcol', 'move', 'm-search', 'notify', 'options', 'patch', 'post', 'purge', 
                            'put', 'report', 'search', 'subscribe', 'trace', 'unlock', 'unsubscribe'];

  //After we call the express constructor
  var newTarget = aspect.afterConstructor(target, {});
  
  var newTarget = newTarget.application;

  aspect.before(newTarget, applicationMethods, function(target, methodName, methodArgs, probeData) {

    that.metricsProbeStart(probeData, target, methodName, methodArgs);
    that.requestProbeStart(probeData, target, methodName, methodArgs);



    if (aspect.findCallbackArg(methodArgs) != undefined) {
     aspect.aroundCallback(methodArgs, probeData, function(target, args, context){

        //Here, the query has executed and returned it's callback. Then
        //stop monitoring
        that.metricsProbeEnd(probeData, methodName, methodArgs);
        that.requestProbeEnd(probeData, methodName, methodArgs);
      });
    }
  });
  return target;
}

/*
 * Lightweight metrics probe for Express queries
 * 
 * These provide:
 *      time:       time event started
 *      route:      the url of the express route 
 *      method:     the HTTP method of the request
 *      duration:   the time for the request to respond
 */
ExpressProbe.prototype.metricsEnd = function(probeData, methodName, methodArgs) {
  probeData.timer.stop();
  am.emit('express', {time: probeData.timer.startTimeMillis, route: methodArgs[0], method: methodName, duration: probeData.timer.timeDelta});
};

/*
 * Heavyweight request probes for Express queries
 */
ExpressProbe.prototype.requestStart = function (probeData, target, methodName, methodArgs) {
  var url = methodArgs[0];
  probeData.req = request.startRequest( 'HTTP', 'request', false, probeData.timer );
  probeData.req.setContext({url: url});
};

ExpressProbe.prototype.requestEnd = function (probeData, methodName, methodArgs) {
  probeData.req.stop({url: methodArgs[0]});
};

module.exports = ExpressProbe;