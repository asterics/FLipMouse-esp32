function ARECommunicator() {
    //The base URI that ARE runs at
    //var _baseURI = "http://" + window.location.host + "/rest/";
    var _baseURI = "http://localhost:8091/rest/";
    var _websocketUrl = "ws://localhost:8092/ws/astericsData";
    var _websocket = null;

    //encodes PathParametes
    function encodeParam(text) {
        var encoded = "";
        for (var i = 0; i < text.length; i++) {
            encoded += text.charCodeAt(i).toString() + '-';
        }
        return encoded;
    }

    this.sendDataToInputPort = function (componentId, portId, value) {
        if (!componentId || !portId || !value) return;

        //use GET sendDataToInputPort() for legacy reasons (phones that do only support GET requests)
        var url = _baseURI + "runtime/model/components/" + encodeParam(componentId) + "/ports/" + encodeParam(portId) + "/data/" + encodeParam(value);
        var xmlHttp = new XMLHttpRequest();
        xmlHttp.open("GET", url, false); // false for synchronous request
        xmlHttp.send(value);
        return new Promise((resolve, reject) => {
            initWebsocket(_websocketUrl).then(function () {
                var result = '';
                var timeoutHandler = setTimeout(function () {
                    resolve(result)
                }, 1000);
                _websocket.onmessage = function (evt) {
                    clearTimeout(timeoutHandler);
                    result += evt.data + '\n';
                    timeoutHandler = setTimeout(function () {
                        console.log("got result: " + result);
                        resolve(result)
                    }, 200);
                };
            }, function () {
                reject();
            });
        });
    };

    function initWebsocket(url) {
        return new Promise((resolve, reject) => {
            if (!_websocket || _websocket.readyState != _websocket.OPEN) {
                if (_websocket) {
                    _websocket.close();
                }
                _websocket = new WebSocket(url);
                _websocket.onopen = function (evt) {
                    console.info("websocket opened!");
                    resolve();
                };
                _websocket.onclose = function (evt) {
                    console.info("websocket closed!");
                };
                _websocket.onerror = function (evt) {
                    console.info("websocket error!");
                    reject();
                };
            } else {
                resolve();
            }
        });
    }
}