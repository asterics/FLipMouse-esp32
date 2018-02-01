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

    var are = new ARECommunicator();
    var _config = {};
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
    var VALUE_AT_CMDS = Object.values(AT_CMD_MAPPING);

    thiz.sendATCmd = function (atCmd) {
        console.log("sending to FlipMouse: " + atCmd);
        return are.sendDataToInputPort('LipMouse.1', 'send', atCmd);
    };

    thiz.setValue = function (valueConstant, value) {
        var atCmd = AT_CMD_MAPPING[valueConstant];
        thiz.sendATCmd(atCmd + ' ' + value);
    };

    thiz.refreshConfig = function () {
        return new Promise((resolve, reject) => {
            thiz.sendATCmd('AT LA').then(function (response) {
                _config = parseConfig(response);
                resolve();
            }, function () {
                reject();
            });
        });
    };

    thiz.save = function () {
        thiz.sendATCmd('AT DE');
        thiz.sendATCmd('AT SA mouse');
    };

    init();

    function init() {
        thiz.refreshConfig().then(function () {
            if (isFunction(initFinished)) {
                initFinished(_config);
            }
        });
    }

    function parseConfig(atCmdsString) {
        return parseConfigElement(atCmdsString.split('\n'));
    }

    function parseConfigElement(remainingList, config) {
        if (!config) {
            config = {};
        }
        if (!remainingList || remainingList.length == 0) {
            return config;
        }
        var currentElement = remainingList[0];
        var nextElement = remainingList[1];
        var currentAtCmd = currentElement.substring(0, AT_CMD_LENGTH);
        if (VALUE_AT_CMDS.includes(currentAtCmd)) {
            config[val2key(currentAtCmd, AT_CMD_MAPPING)] = parseInt(currentElement.substring(5));
        }
        return parseConfigElement(remainingList.slice(1), config);
    }

    function val2key(val, array) {
        for (var key in array) {
            if (array[key] == val) {
                return key;
            }
        }
        return false;
    }

    function isFunction(functionToCheck) {
        var getType = {};
        return functionToCheck && getType.toString.call(functionToCheck) === '[object Function]';
    }
}