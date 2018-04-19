window.tabAction = {};

window.tabAction.init = function () {
    tabAction.initBtnModeActionTable();
    L.removeAllChildren('#selectActionButton');
    C.BTN_MODES.forEach(function (btnMode) {
        var option = L.createElement('option', '', L.translate(btnMode));
        option.value = btnMode;
        L('#selectActionButton').appendChild(option);
    });
    L('#currentAction').innerHTML = getReadable(flip.getConfig(C.BTN_MODES[0]));
    L('#' + C.LEARN_CAT_KEYBOARD).click();
    tabAction.selectMode(flip.getConfig(flip.FLIPMOUSE_MODE), true);

    L('#SELECT_LEARN_CAT_MOUSE').innerHTML = L.createSelectItems(C.AT_CMDS_MOUSE);
    L('#SELECT_LEARN_CAT_FLIPACTIONS').innerHTML = L.createSelectItems(C.AT_CMDS_FLIP);
};

window.tabAction.initBtnModeActionTable = function () {
    L.removeAllChildren('#currentConfigTb');
    var backColor = false;
    C.BTN_MODES.forEach(function (btnMode) {
        var liElm = L.createElement('li', 'row');
        var changeA = L.createElement('a', '', L.translate(btnMode));
        changeA.href = 'javascript:tabAction.selectActionButton("' + btnMode + '")';
        changeA.title = L.translate('CHANGE_TOOLTIP', L.translate(btnMode));
        var descriptionDiv = L.createElement('div', 'two columns', changeA);
        var currentActionDiv = L.createElement('div', 'four columns', getReadable(flip.getConfig(btnMode)));
        var currentAtCmdDiv = L.createElement('div', 'four columns', flip.getConfig(btnMode));
        var spacerDiv = L.createElement('div', 'one column show-mobile space-bottom');
        liElm.appendChild(descriptionDiv);
        liElm.appendChild(currentActionDiv);
        liElm.appendChild(currentAtCmdDiv);
        liElm.appendChild(spacerDiv);
        if(backColor) {
            liElm.style = 'background-color: #e0e0e0;';
        }
        L('#currentConfigTb').appendChild(liElm);
        backColor = !backColor;
    });
};

window.tabAction.selectActionButton = function (btnMode) {
    L('#selectActionButton').value = btnMode;
    refreshCurrentAction(btnMode);
    resetSelects();
    initAdditionalData();
};

function refreshCurrentAction(btnMode) {
    L('#currentAction').innerHTML = getReadable(flip.getConfig(btnMode));
}

window.tabAction.selectActionCategory = function (category) {
    console.log(category);
    L.removeClass('[for*=LEARN_CAT_]', 'color-lightercyan selected');
    L.addClass('[for=' + category + ']', 'color-lightercyan selected');
    L.setVisible('[id^=WRAPPER_LEARN_CAT]', false);
    L.setVisible('#WRAPPER_' + category);

    resetSelects();
    initAdditionalData();
};

window.tabAction.selectMode = function (mode, dontSend) {
    if(!mode) return;
    console.log(mode);
    L.removeClass('[for*=MODE_]', 'color-lightercyan selected');
    L.addClass('[for=' + mode + ']', 'color-lightercyan selected');
    L('#' + mode).checked = true;
    if(!dontSend) {
        flip.setFlipmouseMode(mode);
    }
};

tabAction.selectAtCmd = function (atCmd) {
    tabAction.selectedAtCommand = atCmd;
    initAdditionalData(atCmd);
    if(!C.ADDITIONAL_DATA_CMDS.includes(atCmd)) {
        tabAction.setAtCmd(atCmd);
    }
};

tabAction.setAtCmd = function (atCmd) {
    var selectedButton = L('#selectActionButton').value;
    if(atCmd && selectedButton) {
        flip.setButtonAction(selectedButton, atCmd);
        refreshCurrentAction(L('#selectActionButton').value);
    }
};

tabAction.setAtCmdWithAdditionalData = function (data) {
    tabAction.setAtCmd(tabAction.selectedAtCommand + ' ' + data);
};

function initAdditionalData(atCmd) {
    L.setVisible('#WRAPPER_' + C.ADDITIONAL_FIELD_TEXT, false);
    L.setVisible('#WRAPPER_' + C.ADDITIONAL_FIELD_SELECT, false);
    switch (atCmd) {
        case C.AT_CMD_LOAD_SLOT:
            L.setVisible('#WRAPPER_' + C.ADDITIONAL_FIELD_SELECT);
            L('#' + C.ADDITIONAL_FIELD_SELECT).innerHTML = L.createSelectItems(flip.getSlots());
            L('[for=' + C.ADDITIONAL_FIELD_SELECT + ']')[0].innerHTML = 'Slot';
            break;
    }
}

function resetSelects() {
    var atCmd = flip.getConfig(L('#selectActionButton').value);
    //L('#SELECT_'+ C.LEARN_CAT_KEYBOARD).value = atCmd; //TODO add if implemented
    L('#SELECT_'+ C.LEARN_CAT_MOUSE).value = atCmd;
    L('#SELECT_'+ C.LEARN_CAT_FLIPACTIONS).value = atCmd;
}

tabAction.handleKeyBoardEvent = function (event) {
    window.tabAction.queue = window.tabAction.queue  || [];
    if(event.keyCode == C.JS_KEYCODE_TAB) {
        return; //keyboard navigation should be possible
    }
    if(event.keyCode == 229) {
        tabAction.listenToKeyboardInput = true;
        return;
    }
    event = event || window.event;
    event.preventDefault();
    var queueElem = getQueueElem(event);
    if (C.SUPPORTED_KEYCODES.includes(queueElem.keyCode)) {
        if (!event.repeat && (queueElem.keyCode != C.JS_KEYCODE_BACKSPACE || !getText(tabAction.queue))) {
            tabAction.queue.push(getQueueElem(event));
        } else if (queueElem.keyCode == C.JS_KEYCODE_BACKSPACE) {
            tabAction.queue.pop();
            if (!getText(tabAction.queue)) {
                tabAction.queue = [];
            }
        }
        tabAction.evalRec();
    }
};

tabAction.handleOnKeyboardInput = function (event) {
    window.tabAction.queue = window.tabAction.queue  || [];
    if(!tabAction.listenToKeyboardInput || tabAction.lastInputLength >= event.target.value.length) {
        if(tabAction.lastInputLength > event.target.value.length) {
            tabAction.resetRec();
        }
        return;
    }
    tabAction.listenToKeyboardInput = false;
    Array.from(event.data).forEach(function (char) {
        var keyCode = char.toUpperCase().charCodeAt(0);
        tabAction.queue.push({
            key: char,
            keyCode: keyCode
        });
    });
    tabAction.evalRec();
};

tabAction.addSpecialKey = function (keycode) {
    window.tabAction.queue = window.tabAction.queue  || [];
    tabAction.queue.push({
        key: 'SPECIAL',
        keyCode: keycode
    });
    tabAction.evalRec();
    L('#INPUT_LEARN_CAT_KEYBOARD').focus();
};

tabAction.evalRec = function () {
    var atCmd = getAtCmd(tabAction.queue);
    L('#buttonRecOK').disabled = !atCmd;
    L('#recordedAtCmd').innerHTML = atCmd || L.translate('NONE_BRACKET');
    var readable = getReadable(atCmd);
    L('#INPUT_LEARN_CAT_KEYBOARD').value = readable;
    tabAction.lastInputLength = readable.length;
};

tabAction.resetRec = function () {
    tabAction.lastInputLength = 0;
    tabAction.queue = [];
    tabAction.evalRec();
};

tabAction.saveRec = function () {
    if(document.onkeydown) {
        document.onkeydown = null;
        L.toggle('#start-rec-button-normal', '#start-rec-button-rec');
    }
    tabAction.setAtCmd(getAtCmd(tabAction.queue));
};

function getQueueElem(event) {
    return {
        key: event.key,
        keyCode: event.keyCode || e.which,
        altKey: event.altKey
    }
}

function isPrintableKey(e) {
    return e.key.length === 1;
}

function isSpecialKey(e) {
    return e.key.length !== 1;
}

/**
 * parses the given keyList and returns the parsed text
 * @param keyList
 * @return the text that is specified by the given keyList, or null if it contains keypress events that do not produce a text
 */
function getText(keyList) {
    var text = '';
    for(var i=0; i<keyList.length; i++) {
        var elm = keyList[i];
        var code = elm.keyCode;
        if(isSpecialKey(elm)) {
            var isAltGr = isAltGrLetter(elm, keyList[i+1], keyList[i+2]);
            if(code != C.JS_KEYCODE_SHIFT && !isAltGr) {
                return null;
            }
            if(isAltGr) {
                i++; //Skip next letter (Alt), continue with printable letter
            }
        } else {
            text += elm.key;
        }
    }
    return text;
}

function isAltGrLetter(e1,e2,e3) {
    if(!e1 || !e2 || !e3) return false;
    return e1.keyCode == C.JS_KEYCODE_CTRL && e2.keyCode == C.JS_KEYCODE_ALT && isPrintableKey(e3) && e3.keyCode && e3.altKey;
}

function getAtCmd(queue) {
    if(!queue || queue.length == 0) {
        return '';
    }
    var atCmd;
    var text = getText(queue);
    if(text) {
        atCmd = C.AT_CMD_WRITEWORD + ' ' + text;
    } else {
        var postfix = '';
        queue.forEach(function (e) {
            var code = C.KEYCODE_MAPPING[e.keyCode];
            if(code) {
                postfix += code + ' ';
            }
        });
        atCmd = C.AT_CMD_KEYPRESS + ' ' + postfix.trim();
    }
    return atCmd.substring(0, C.MAX_LENGTH_ATCMD);
}

function getReadable(atCmd) {
    if(!atCmd) {
        return '';
    }
    var prefix = atCmd.substring(0, C.LENGTH_ATCMD_PREFIX-1);
    var postfix = atCmd.substring(C.LENGTH_ATCMD_PREFIX);
    if(prefix == C.AT_CMD_KEYPRESS) {
        postfix = postfix.replace(/ /g, ' + ');
    }
    return L.translate(prefix, postfix);
}