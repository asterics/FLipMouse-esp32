window.ws = {};

/**
 * inits a websocket by a given url, returned promise resolves with initialized websocket, rejects after failure/timeout.
 *
 * @param url the websocket url to init
 * @param existingWebsocket if passed and this passed websocket is already open, this existingWebsocket is resolved, no additional websocket is opened
 * @param timeoutMs the timeout in milliseconds for opening the websocket
 * @param numberOfRetries the number of times initializing the socket should be retried, if not specified or 0, no retries are made
 *        and a failure/timeout causes rejection of the returned promise
 * @return {Promise}
 */
ws.initWebsocket = function(url, existingWebsocket, timeoutMs, numberOfRetries) {
    timeoutMs = timeoutMs ? timeoutMs : 1500;
    numberOfRetries = numberOfRetries ? numberOfRetries : 0;
    var hasReturned = false;
    var promise = new Promise(function(resolve, reject) {
        setTimeout(function () {
            if(!hasReturned) {
                console.info('opening websocket timed out: ' + url);
                rejectInternal();
            }
        }, timeoutMs);
        if (!existingWebsocket || existingWebsocket.readyState != existingWebsocket.OPEN) {
            if (existingWebsocket) {
                existingWebsocket.close();
            }
            var websocket = new WebSocket(url);
            websocket.onopen = function () {
                if(hasReturned) {
                    websocket.close();
                } else {
                    console.info('websocket to opened! url: ' + url);
                    resolve(websocket);
                }
            };
            websocket.onclose = function () {
                console.info('websocket closed! url: ' + url);
                rejectInternal();
            };
            websocket.onerror = function () {
                console.info('websocket error! url: ' + url);
                rejectInternal();
            };
        } else {
            resolve(existingWebsocket);
        }

        function rejectInternal() {
            if(numberOfRetries <= 0) {
                reject();
            } else if(!hasReturned) {
                hasReturned = true;
                console.info('retrying connection to websocket! url: ' + url + ', remaining retries: ' + (numberOfRetries-1));
                ws.initWebsocket(url, null, timeoutMs, numberOfRetries-1).then(resolve, reject);
            }
        }
    });
    promise.then(function () {hasReturned = true;}, function () {hasReturned = true;});
    return promise;
};

/**
 * handles got from flipmouse over a websocket. Data containing live values are passed to valueHandler function, other data resolves
 * the returned promise.
 * @param socket
 * @param valueHandler
 * @param timeout maximum time after the promise is resolved (optional). Default 3000ms.
 * @return {Promise}
 */
ws.handleData = function (socket, valueHandler, timeout) {
    timeout = timeout || 3000;
    return new Promise(function(resolve) {
        var result = '';
        var timeoutHandler = setTimeout(function () {
            //console.log("timeout von command: " + value);
            resolve(result);
        }, timeout);
        socket.onmessage = function (evt) {
            if (evt.data && evt.data.indexOf(C.LIVE_VALUE_CONSTANT) > -1) {
                if (L.isFunction(valueHandler)) {
                    valueHandler(evt.data);
                }
                return;
            }
            clearTimeout(timeoutHandler);
            result += evt.data + '\n';
            timeoutHandler = setTimeout(function () {
                console.log("got result: " + result);
                resolve(result)
            }, 200);
        };
    });
};