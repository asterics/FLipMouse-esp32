function ARECommunicator() {
    //The base URI that ARE runs at
    //var _baseURI = "http://" + window.location.host + "/rest/";
    var VALUE_CONSTANT = 'VALUES:';
    var _baseURI = 'http://' + window.location.hostname + ':8091/rest/';
    var _websocketUrl = 'ws://' + window.location.hostname + ':8092/ws/astericsData';
    var _websocket = null;
    var _valueHandler = null;
    var _component = 'LipMouse.1';
    var _inputPort = 'send';

    //encodes PathParametes
    function encodeParam(text) {
        var encoded = "";
        for (var i = 0; i < text.length; i++) {
            encoded += text.charCodeAt(i).toString() + '-';
        }
        return encoded;
    }

    this.setValueHandler = function (handler) {
        _valueHandler = handler;
    };

    this.sendData = function (value) {
        if (!value) return;

        //use GET sendDataToInputPort() for legacy reasons (phones that do only support GET requests)
        var url = _baseURI + "runtime/model/components/" + encodeParam(_component) + "/ports/" + encodeParam(_inputPort) + "/data/" + encodeParam(value);
        var xmlHttp = new XMLHttpRequest();
        xmlHttp.open("GET", url); // false for synchronous request
        xmlHttp.send(value);
        return new Promise((resolve, reject) => {
            initWebsocket(_websocketUrl).then(function () {
                var result = '';
                var timeoutHandler = setTimeout(function () {
                    //console.log("timeout von command: " + value);
                    resolve(result);
                }, 3000);
                _websocket.onmessage = function (evt) {
                    if (evt.data && evt.data.indexOf(VALUE_CONSTANT) > -1) {
                        if (L.isFunction(_valueHandler)) {
                            _valueHandler(evt.data);
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