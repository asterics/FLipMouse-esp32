function ARECommunicator(socket) {
    var _restURI = 'http://' + window.location.hostname + ':8091/rest/';
    var _websocket = socket;
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

    this.sendData = function (value, timeout) {
        if (!value) return;

        //use GET sendDataToInputPort() for legacy reasons (phones that do only support GET requests)
        var url = _restURI + "runtime/model/components/" + encodeParam(_component) + "/ports/" + encodeParam(_inputPort) + "/data/" + encodeParam(value);
        var xmlHttp = new XMLHttpRequest();
        xmlHttp.open("GET", url);
        xmlHttp.send(value);
        return new Promise(function (resolve, reject) {
            ws.initWebsocket(C.ARE_WEBSOCKET_URL, _websocket).then(function (socket) {
                _websocket = socket;
                ws.handleData(_websocket, _valueHandler, timeout).then(resolve);
            }, function () {
                reject();
            });
        });
    };
}