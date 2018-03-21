window.ws = {};

/**
 * inits a websocket by a given url, resolves with initialized websocket
 * @param url the websocket url to init
 * @param existingWebsocket if passed and this passed websocket is already open, this existingWebsocket is resolved, no additional websocket is opened
 * @return {Promise}
 */
ws.initWebsocket = function(url, existingWebsocket) {
    return new Promise((resolve, reject) => {
        if (!existingWebsocket || existingWebsocket.readyState != existingWebsocket.OPEN) {
            if (existingWebsocket) {
                existingWebsocket.close();
            }
            var websocket = new WebSocket(url);
            websocket.onopen = function () {
                console.info('websocket to opened! url: ' + url);
                resolve(websocket);
            };
            websocket.onclose = function () {
                console.info('websocket closed! url: ' + url);
            };
            websocket.onerror = function () {
                console.info('websocket error! url: ' + url);
                reject();
            };
        } else {
            resolve(existingWebsocket);
        }
    });
};

/**
 * handles got from flipmouse over a websocket. Data containing live values are passed to valueHandler function, other data resolves
 * the returned promise. Each promise is resolved after a maximum of 3000ms, independent of data got over the websocket.
 * @param socket
 * @param valueHandler
 * @return {Promise}
 */
ws.handleData = function (socket, valueHandler) {
    return new Promise(resolve => {
        var result = '';
        var timeoutHandler = setTimeout(function () {
            //console.log("timeout von command: " + value);
            resolve(result);
        }, 3000);
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