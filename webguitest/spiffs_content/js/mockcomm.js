function MockCommunicator() {
    var VALUE_CONSTANT = 'VALUES:';
    var _valueHandler = null;
    var _invervalHandler = null;
    var thiz = this;

    this.setValueHandler = function (handler) {
        _valueHandler = handler;
    };

    this.sendData = function (value) {
        if (!value) return;
        thiz.pressure = thiz.pressure || 500;
        thiz.x = thiz.x || 0;
        thiz.y = thiz.y || 0;
        thiz.incrementP = thiz.incrementP || 1;
        thiz.incrementXY = thiz.increment || 1;

        return new Promise(function (resolve) {
            if (value == 'AT') {
                resolve('');
            } else if (value.indexOf('AT SR') > -1) {
                _invervalHandler = setInterval(function () {
                    if (L.isFunction(_valueHandler)) {
                        thiz.pressure += thiz.incrementP;
                        thiz.x += thiz.incrementXY*getRandomInt(-5,5);
                        thiz.y += thiz.incrementXY*(-1)*getRandomInt(-5,5);
                        if(thiz.pressure > 550 || thiz.pressure < 450) {
                            thiz.incrementP *= -1;
                        }
                        if(thiz.y > 100 || thiz.y < -100 || thiz.x > 100 || thiz.x < -100) {
                            thiz.incrementXY *= -1;
                        }
                        _valueHandler(VALUE_CONSTANT + thiz.pressure + ',0,0,0,0,' + thiz.x + ',' + thiz.y);
                    }
                }, 200);
            } else if (value.indexOf('AT ER') > -1) {
                clearInterval(_invervalHandler);
            } else if (value.indexOf('AT LA') > -1) {
                var cmds = C.DEFAULT_CONFIGURATION.join('\n');
                cmds = 'Slot:mouse\n' + cmds + '\nEND';
                resolve(cmds);
            } else if (value.indexOf('AT CA') > -1) {
                thiz.x = 0;
                thiz.y = 0;
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