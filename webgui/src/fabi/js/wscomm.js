function WsCommunicator(wsUrl, socket) {
    var _websocket = socket;
    var _valueHandler = null;

    this.setValueHandler = function (handler) {
        _valueHandler = handler;
    };

    this.sendData = function (value, timeout) {
        if (!value) return;
        return new Promise(function(resolve, reject) {
            ws.initWebsocket(wsUrl, _websocket).then(function (socket) {
                socket.send(value);
                _websocket = socket;
                ws.handleData(_websocket, _valueHandler, timeout).then(resolve);
            }, function () {
                reject();
            });
        });
    };
}