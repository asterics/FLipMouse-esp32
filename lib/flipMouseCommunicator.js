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

    thiz.BTN_MODE_BUTTON_1 = 'BTN_MODE_BUTTON_1';
    thiz.BTN_MODE_BUTTON_2 = 'BTN_MODE_BUTTON_2';
    thiz.BTN_MODE_BUTTON_3 = 'BTN_MODE_BUTTON_3';
    thiz.BTN_MODE_STICK_UP = 'BTN_MODE_STICK_UP';
    thiz.BTN_MODE_STICK_DOWN = 'BTN_MODE_STICK_DOWN';
    thiz.BTN_MODE_STICK_LEFT = 'BTN_MODE_STICK_LEFT';
    thiz.BTN_MODE_STICK_RIGHT = 'BTN_MODE_STICK_RIGHT';
    thiz.BTN_MODE_SIP = 'BTN_MODE_SIP';
    thiz.BTN_MODE_PUFF = 'BTN_MODE_PUFF';
    thiz.BTN_MODES = [thiz.BTN_MODE_BUTTON_1, thiz.BTN_MODE_BUTTON_2, thiz.BTN_MODE_BUTTON_3,
        thiz.BTN_MODE_STICK_UP, thiz.BTN_MODE_STICK_DOWN, thiz.BTN_MODE_STICK_LEFT, thiz.BTN_MODE_STICK_RIGHT,
        thiz.BTN_MODE_SIP, thiz.BTN_MODE_PUFF];
    var AT_CMD_BTN_MODE = 'AT BM';

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
    var _AT_CMD_MIN_WAITTIME_MS = 50;
    var _timestampLastAtCmd = new Date().getTime();
    var _atCmdQueue = [];
    var _sendingAtCmds = false;

    /**
     * sends the given AT command to the FLipMouse. If sending of the last command is not completed yet, the given AT command
     * is added to a queue and will be sent later. The time between two sent commands is at least _AT_CMD_MIN_WAITTIME_MS.
     * The order of sending the commands is always equal to the order of calls to this function.
     *
     * @param atCmd
     * @return {Promise}
     */
    thiz.sendATCmd = function (atCmd) {
        var promise = new Promise((resolve) => {
            if (_atCmdQueue.length > 0) {
                console.log("adding cmd to queue: " + atCmd);
            }
            _atCmdQueue.push({
                cmd: atCmd,
                resolveFn: resolve
            });
        });
        if (!_sendingAtCmds) {
            sendNext();
        }

        function sendNext() {
            _sendingAtCmds = true;
            if (_atCmdQueue.length == 0) {
                _sendingAtCmds = false;
                return;
            }
            var nextCmd = _atCmdQueue.shift();
            var timeout = _AT_CMD_MIN_WAITTIME_MS - (new Date().getTime() - _timestampLastAtCmd);
            timeout = timeout > 0 ? timeout : 0;
            //console.log("waiting for cmd: " + nextCmd.cmd + ", " + timeout + "ms");
            setTimeout(function () {
                _timestampLastAtCmd = new Date().getTime();
                console.log("sending to FlipMouse: " + nextCmd.cmd);
                are.sendDataToInputPort('LipMouse.1', 'send', nextCmd.cmd).then(nextCmd.resolveFn);
                sendNext();
            }, timeout);
        }

        return promise;
    };

    thiz.testConnection = function () {
        return new Promise((resolve) => {
            thiz.sendATCmd('AT').then(function (response) {
                resolve(response.indexOf('OK') > -1 ? true : false);
            }, function () {
                resolve(false);
            });
        });
    };

    thiz.setValue = function (valueConstant, value, debounceTimeout) {
        if (!debounceTimeout) {
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

    /**
     * saves the complete current configuration (all slots) to the FLipMouse. This is done by the following steps:
     * 1) Deleting alls slots
     * 2) loading slot configuration to FLipMouse
     * 3) saving slot
     * 4) go back to (2) for next slot, until all slots are saved
     * 5) loading slot configuration to FLipMouse that was loaded before saving
     *
     * @return {Promise}
     */
    thiz.save = function (updateProgressHandler) {
        updateProgressHandler = updateProgressHandler || function () {
        };
        var progress = 0;
        sendAtCmdNoResultHandling('AT DE');
        thiz.pauseLiveValueListener();
        increaseProgress(10);
        var percentPerSlot = 50 / thiz.getSlots().length;
        var saveSlotsPromise = new Promise(function (resolve) {
            loadAndSaveSlot(thiz.getSlots(), 0);

            function loadAndSaveSlot(slots, i) {
                if (i >= slots.length) {
                    resolve();
                    return;
                }
                var slot = slots[i];
                thiz.testConnection().then(function () {
                    loadSlotByConfig(slot);
                    thiz.testConnection().then(function () {
                        sendAtCmdNoResultHandling('AT SA ' + slot);
                        increaseProgress(percentPerSlot);
                        loadAndSaveSlot(slots, i + 1);
                    });
                });
            }
        });

        function increaseProgress(percent) {
            progress += percent;
            updateProgressHandler(progress);
        }

        return new Promise(function (resolve) {
            saveSlotsPromise.then(function () {
                thiz.testConnection().then(function () {
                    increaseProgress(30);
                    loadSlotByConfig(_currentSlot).then(function () {
                        increaseProgress(10);
                        thiz.resumeLiveValueListener();
                        resolve();
                    });
                });
            });
        });
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

    thiz.pauseLiveValueListener = function () {
        thiz.sendATCmd('AT ER');
        console.log('listening to live values stopped.');
    };

    thiz.resumeLiveValueListener = function () {
        if (_valueHandler) {
            thiz.sendATCmd('AT SR');
            console.log('listening to live values resumed.');
        } else {
            console.log('listening to live values not resumed, because no value handler.');
        }
    };

    thiz.getConfig = function (constant, slot) {
        slot = slot || _currentSlot;
        return _config[slot] ? _config[slot][constant] : null;
    };

    thiz.setConfig = function (constant, value, slot) {
        slot = slot || _currentSlot;
        if (_config[slot]) {
            _config[slot][constant] = value;
        }
    };

    thiz.getSlots = function () {
        return Object.keys(_config);
    };

    thiz.getCurrentSlot = function () {
        return _currentSlot;
    };

    thiz.setSlot = function (slot) {
        if (thiz.getSlots().includes(slot)) {
            _currentSlot = slot;
            sendAtCmdNoResultHandling('AT LO ' + slot);
        }
        return _config[_currentSlot];
    };

    thiz.createSlot = function (slotName, progressHandler) {
        if (!slotName || thiz.getSlots().includes(slotName)) {
            console.warn('slot not saved because no slot name or slot already existing!');
        }
        _config[slotName] = L.deepCopy(_config[_currentSlot]);
        _currentSlot = slotName;
        sendAtCmdNoResultHandling('AT SA ' + slotName);
        if (progressHandler) {
            progressHandler(50);
        }
        return thiz.testConnection();
    };

    thiz.deleteSlot = function (slotName, progressHandler) {
        if (!slotName || !thiz.getSlots().includes(slotName)) {
            console.warn('slot not deleted because no slot name or slot not existing!');
        }
        delete _config[slotName];
        if(slotName == _currentSlot) {
            _currentSlot = thiz.getSlots()[0];
        }
        return thiz.save(progressHandler);
    };

    thiz.getLiveData = function (constant) {
        if (constant) {
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

    function setLiveValueHandler(handler) {
        _valueHandler = handler;
        if (L.isFunction(_valueHandler)) {
            sendAtCmdNoResultHandling('AT SR');
            are.setValueHandler(parseLiveValues);
        } else {
            sendAtCmdNoResultHandling('AT ER');
        }
    }

    function parseLiveValues(data) {
        if (!L.isFunction(_valueHandler)) {
            are.setValueHandler(null);
            return;
        }
        if (!data || data.indexOf('VALUES') == -1) {
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
        thiz.sendATCmd(atCmd).then(function () {
        }, function () {
        });
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

        if (currentElement.indexOf(_SLOT_CONSTANT) > -1) {
            var slot = currentElement.substring(_SLOT_CONSTANT.length);
            if (!_currentSlot) {
                _currentSlot = slot;
            }
            config = {};
            _config[slot] = config;
        } else {
            var currentAtCmd = currentElement.substring(0, AT_CMD_LENGTH);
            if (VALUE_AT_CMDS.includes(currentAtCmd)) {
                config[L.val2key(currentAtCmd, AT_CMD_MAPPING)] = parseInt(currentElement.substring(AT_CMD_LENGTH));
            } else if(currentAtCmd.indexOf(AT_CMD_BTN_MODE) > -1) {
                var buttonModeIndex = parseInt(currentElement.substring(AT_CMD_LENGTH));
                if(thiz.BTN_MODES[buttonModeIndex-1]) {
                    config[thiz.BTN_MODES[buttonModeIndex-1]] = nextElement.trim();
                }
            }
        }
        return parseConfigElement(remainingList.slice(1), config);
    }

    function loadSlotByConfig(slotName) {
        var config = _config[slotName];
        return new Promise(function (resolve) {
            var promises = [];
            Object.keys(config).forEach(function (key) {
                var atCmd = AT_CMD_MAPPING[key];
                if(thiz.BTN_MODES.includes(key)) {
                    var index = thiz.BTN_MODES.indexOf(key) + 1;
                    var indexFormatted = ("0" + index).slice(-2); //1 => 01
                    atCmd = 'AT BM ' + indexFormatted;
                    promises.push(thiz.sendATCmd(atCmd));
                    promises.push(thiz.sendATCmd(config[key]));
                } else {
                    promises.push(thiz.sendATCmd(atCmd + ' ' + config[key]));
                }
            });
            Promise.all(promises).then(function () {
                resolve();
            });
        });
    }
}