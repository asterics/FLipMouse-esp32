function MockCommunicator() {
    var VALUE_CONSTANT = 'VALUES:';
    var _valueHandler = null;
    var _invervalHandler = null;

    this.setValueHandler = function (handler) {
        _valueHandler = handler;
    };

    this.sendData = function (value) {
        if (!value) return;

        return new Promise(function (resolve) {
            if (value == 'AT') {
                resolve('OK');
            } else if (value.indexOf('AT SR') > -1) {
                _invervalHandler = setInterval(function () {
                    if (L.isFunction(_valueHandler)) {
                        var x = getRandomInt2(50);
                        var y = getRandomInt2(50);
                        _valueHandler(VALUE_CONSTANT + '500,0,0,0,0,' + x + ',' + y);
                    }
                }, 200);
            } else if (value.indexOf('AT ER') > -1) {
                clearInterval(_invervalHandler);
            } else if (value.indexOf('AT LA') > -1) {
                var cmds = C.DEFAULT_CONFIGURATION.join('\n');
                cmds = 'Slot:mouse\n' + cmds + '\nEND';
                resolve(cmds);
            }
            setTimeout(function () {
                resolve();
            }, 3000);
        });
    };
}

function getRandomInt(min, max) {
    return Math.floor(Math.random() * (max - min + 1)) + min;
}

function getRandomInt2(factor) {
    return Math.floor((Math.random() - Math.random())*factor);
}