function FlipMouse(initFinished) {
    var thiz = this;
    thiz.AT_CMD_MAPPING = {};
    thiz.SENSITIVITY_X = 'SENSITIVITY_X';
    thiz.SENSITIVITY_Y = 'SENSITIVITY_Y';
    thiz.ACCELERATION = 'ACCELERATION';
    thiz.MAX_SPEED = 'MAX_SPEED';
    thiz.DEADZONE_X = 'DEADZONE_X';
    thiz.DEADZONE_Y = 'DEADZONE_Y';
    thiz.SIP_THRESHOLD = 'SIP_THRESHOLD';
    thiz.SIP_STRONG_THRESHOLD = 'SIP_STRONG_THRESHOLD';
    thiz.PUFF_THRESHOLD = 'PUFF_THRESHOLD';
    thiz.PUFF_STRONG_THRESHOLD = 'PUFF_STRONG_THRESHOLD';
    thiz.ORIENTATION_ANGLE = 'ORIENTATION_ANGLE';

    thiz.LIVE_PRESSURE = 'LIVE_PRESSURE';
    thiz.LIVE_UP = 'LIVE_UP';
    thiz.LIVE_DOWN = 'LIVE_DOWN';
    thiz.LIVE_LEFT = 'LIVE_LEFT';
    thiz.LIVE_RIGHT = 'LIVE_RIGHT';
    thiz.LIVE_MOV_X = 'LIVE_MOV_X';
    thiz.LIVE_MOV_Y = 'LIVE_MOV_Y';
    thiz.LIVE_MOV_X_MIN = 'LIVE_MOV_X_MIN';
    thiz.LIVE_MOV_X_MAX = 'LIVE_MOV_X_MAX';
    thiz.LIVE_MOV_Y_MIN = 'LIVE_MOV_Y_MIN';
    thiz.LIVE_MOV_Y_MAX = 'LIVE_MOV_Y_MAX';
    thiz.LIVE_PRESSURE_MIN = 'LIVE_PRESSURE_MIN';
    thiz.LIVE_PRESSURE_MIN = 'LIVE_PRESSURE_MIN';
    thiz.LIVE_PRESSURE_MAX = 'LIVE_PRESSURE_MAX';

    thiz.SIP_PUFF_IDS = [
        L.getIDSelector(thiz.SIP_THRESHOLD),
        L.getIDSelector(thiz.SIP_STRONG_THRESHOLD),
        L.getIDSelector(thiz.PUFF_THRESHOLD),
        L.getIDSelector(thiz.PUFF_STRONG_THRESHOLD)
    ];

    var are = new ARECommunicator();
    var _config = {};
    var _liveData = {};
    var AT_CMD_LENGTH = 5;

    var AT_CMD_MAPPING = {};
    AT_CMD_MAPPING[thiz.SENSITIVITY_X] = 'AT AX';
    AT_CMD_MAPPING[thiz.SENSITIVITY_Y] = 'AT AY';
    AT_CMD_MAPPING[thiz.ACCELERATION] = 'AT AC';
    AT_CMD_MAPPING[thiz.MAX_SPEED] = 'AT MS';
    AT_CMD_MAPPING[thiz.DEADZONE_X] = 'AT DX';
    AT_CMD_MAPPING[thiz.DEADZONE_Y] = 'AT DY';
    AT_CMD_MAPPING[thiz.SIP_THRESHOLD] = 'AT TS';
    AT_CMD_MAPPING[thiz.SIP_STRONG_THRESHOLD] = 'AT SS';
    AT_CMD_MAPPING[thiz.PUFF_THRESHOLD] = 'AT TP';
    AT_CMD_MAPPING[thiz.PUFF_STRONG_THRESHOLD] = 'AT SP';
    AT_CMD_MAPPING[thiz.ORIENTATION_ANGLE] = 'AT RO';
    var VALUE_AT_CMDS = Object.values(AT_CMD_MAPPING);
    var debouncers = {};
    var _valueHandler = null;
    var _currentSlot = null;
    var _SLOT_CONSTANT = 'Slot:';

    thiz.sendATCmd = function (atCmd) {
        console.log("sending to FlipMouse: " + atCmd);
        return are.sendDataToInputPort('LipMouse.1', 'send', atCmd);
    };

    thiz.testConnection = function () {
        return new Promise((resolve, reject) => {
            thiz.sendATCmd('AT').then(function () {
                resolve(true);
            }, function () {
                resolve(false);
            });
        });
    };

    thiz.setValue = function (valueConstant, value, debounceTimeout) {
        if(!debounceTimeout) {
            debounceTimeout = 300;
        }
        thiz.setConfig(valueConstant, parseInt(value));
        clearInterval(debouncers[valueConstant]);
        debouncers[valueConstant] = setTimeout(function () {
            var atCmd = AT_CMD_MAPPING[valueConstant];
            sendAtCmdNoResultHandling(atCmd + ' ' + value);
        }, debounceTimeout);
    };

    thiz.refreshConfig = function () {
        return new Promise((resolve, reject) => {
            thiz.sendATCmd('AT LA').then(function (response) {
                parseConfig(response);
                resolve();
            }, function () {
                console.log("could not get config!");
                reject();
            });
        });
    };

    thiz.save = function () {
        sendAtCmdNoResultHandling('AT DE');
        sendAtCmdNoResultHandling('AT SA mouse');
        return thiz.testConnection();
    };

    thiz.calibrate = function () {
        sendAtCmdNoResultHandling('AT CA');
        return thiz.testConnection();
    };

    thiz.rotate = function () {
        var currentOrientation = thiz.getConfig(thiz.ORIENTATION_ANGLE);
        thiz.setValue(thiz.ORIENTATION_ANGLE, (currentOrientation + 90) % 360, 0);
        sendAtCmdNoResultHandling('AT CA');
        return thiz.testConnection();
    };

    thiz.startLiveValueListener = function (handler) {
        console.log('starting listening to live values...');
        setLiveValueHandler(handler);
    };

    thiz.stopLiveValueListener = function () {
        setLiveValueHandler(null);
        console.log('listening to live values stopped.');
    };

    thiz.getConfig = function (constant, slot) {
        slot = slot || _currentSlot;
        return _config[slot] ? _config[slot][constant] : null;
    };

    thiz.setConfig = function (constant, value, slot) {
        slot = slot || _currentSlot;
        if(_config[slot]) {
            _config[slot][constant] = value;
        }
    };

    thiz.getLiveData = function (constant) {
        if(constant) {
            return _liveData[constant];
        }
        return _liveData;
    };

    thiz.resetMinMaxLiveValues = function () {
        _liveData[thiz.LIVE_PRESSURE_MIN] = 1024;
        _liveData[thiz.LIVE_MOV_X_MIN] = 1024;
        _liveData[thiz.LIVE_MOV_Y_MIN] = 1024;
        _liveData[thiz.LIVE_PRESSURE_MAX] = -1;
        _liveData[thiz.LIVE_MOV_X_MAX] = -1;
        _liveData[thiz.LIVE_MOV_Y_MAX] = -1;
    };

    init();
    function init() {
        thiz.resetMinMaxLiveValues();
        thiz.refreshConfig().then(function () {
            if (L.isFunction(initFinished)) {
                initFinished(_config[_currentSlot]);
            }
        }, function () {
        });
    }

    function setLiveValueHandler (handler) {
        _valueHandler = handler;
        if(L.isFunction(_valueHandler)) {
            sendAtCmdNoResultHandling('AT SR');
            are.setValueHandler(parseLiveValues);
        } else {
            sendAtCmdNoResultHandling('AT ER');
        }
    }

    function parseLiveValues(data) {
        if(!L.isFunction(_valueHandler)) {
            are.setValueHandler(null);
            return;
        }
        if(!data || data.indexOf('VALUES') == -1) {
            console.log('error parsing live data: ' + data);
            return;
        }

        var valArray = data.split(':')[1].split(',');
        _liveData[thiz.LIVE_PRESSURE] = parseInt(valArray[0]);
        _liveData[thiz.LIVE_UP] = parseInt(valArray[1]);
        _liveData[thiz.LIVE_DOWN] = parseInt(valArray[2]);
        _liveData[thiz.LIVE_LEFT] = parseInt(valArray[3]);
        _liveData[thiz.LIVE_RIGHT] = parseInt(valArray[4]);
        _liveData[thiz.LIVE_MOV_X] = parseInt(valArray[5]);
        _liveData[thiz.LIVE_MOV_Y] = parseInt(valArray[6]);
        _liveData[thiz.LIVE_PRESSURE_MIN] = Math.min(_liveData[thiz.LIVE_PRESSURE_MIN], _liveData[thiz.LIVE_PRESSURE]);
        _liveData[thiz.LIVE_MOV_X_MIN] = Math.min(_liveData[thiz.LIVE_MOV_X_MIN], _liveData[thiz.LIVE_MOV_X]);
        _liveData[thiz.LIVE_MOV_Y_MIN] = Math.min(_liveData[thiz.LIVE_MOV_Y_MIN], _liveData[thiz.LIVE_MOV_Y]);
        _liveData[thiz.LIVE_PRESSURE_MAX] = Math.max(_liveData[thiz.LIVE_PRESSURE_MAX], _liveData[thiz.LIVE_PRESSURE]);
        _liveData[thiz.LIVE_MOV_X_MAX] = Math.max(_liveData[thiz.LIVE_MOV_X_MAX], _liveData[thiz.LIVE_MOV_X]);
        _liveData[thiz.LIVE_MOV_Y_MAX] = Math.max(_liveData[thiz.LIVE_MOV_Y_MAX], _liveData[thiz.LIVE_MOV_Y]);

        _valueHandler(_liveData);
    }

    function sendAtCmdNoResultHandling(atCmd) {
        thiz.sendATCmd(atCmd).then(function () {}, function () {});
    }

    function parseConfig(atCmdsString) {
        return parseConfigElement(atCmdsString.split('\n'));
    }

    function parseConfigElement(remainingList, config) {
        if (!remainingList || remainingList.length == 0) {
            return _config;
        }
        config = config || {};
        var currentElement = remainingList[0];
        var nextElement = remainingList[1];

        if(currentElement.indexOf(_SLOT_CONSTANT) > -1) {
            var slot = currentElement.substring(_SLOT_CONSTANT.length);
            if (!_currentSlot) {
                _currentSlot = slot;
            }
            config = {};
            _config[slot] = config;
        } else {
            var currentAtCmd = currentElement.substring(0, AT_CMD_LENGTH);
            if (VALUE_AT_CMDS.includes(currentAtCmd)) {
                config[L.val2key(currentAtCmd, AT_CMD_MAPPING)] = parseInt(currentElement.substring(5));
            }
        }
        return parseConfigElement(remainingList.slice(1), config);
    }
}