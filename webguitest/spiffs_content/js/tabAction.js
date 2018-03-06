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
    console.log(btnMode);
    L('#selectActionButton').value = btnMode;
    L('#currentAction').innerHTML = getReadable(flip.getConfig(btnMode));
};

window.tabAction.selectActionCategory = function (category) {
    console.log(category);
    L.removeClass('[for*=LEARN_CAT_]', 'color-lightercyan selected');
    L.addClass('[for=' + category + ']', 'color-lightercyan selected');
};

window.tabAction.selectMode = function (mode, dontSend) {
    console.log(mode);
    L.removeClass('[for*=MODE_]', 'color-lightercyan selected');
    L.addClass('[for=' + mode + ']', 'color-lightercyan selected');
    L('#' + mode).checked = true;
    if(!dontSend) {
        flip.setFlipmouseMode(mode);
    }
};

window.tabAction.startRec = function () {
    if(!document.onkeydown) {
        L('#recordedAction').innerHTML = '';
        L('#recordedAtCmd').innerHTML = '';
        window.tabAction.queue = [];
        document.onkeydown = function(e) {
            e = e || window.event;
            e.preventDefault();
            if(!e.repeat && C.SUPPORTED_KEYCODES.includes(getKeycode(e))) {
                if(getKeycode(e) != C.JS_KEYCODE_BACKSPACE || !getText(tabAction.queue)) {
                    tabAction.queue.push(e);
                } else {
                    tabAction.queue.pop();
                    if(!getText(tabAction.queue)) {
                        tabAction.queue = [];
                    }
                }
                var atCmd = getAtCmd(tabAction.queue);
                L('#recordedAction').innerHTML = getReadable(atCmd);
                L('#recordedAtCmd').innerHTML = atCmd;
            }
        };
    } else {
        var atCmd = getAtCmd(tabAction.queue);
        var selectedButton = L('#selectActionButton').value;
        if(atCmd && selectedButton) {
            flip.setButtonAction(selectedButton, atCmd);
        }
        document.onkeydown = null;
    }
};

function getKeycode(e) {
    if(L.equalIgnoreCase(e.key, 'ö')) {
        return C.JS_KEYCODE_OE;
    } else if (L.equalIgnoreCase(e.key, 'ü')) {
        return C.JS_KEYCODE_UE;
    } else if (L.equalIgnoreCase(e.key, 'ä')) {
        return C.JS_KEYCODE_AE;
    } else if (['ß', '?', '\\'].includes(e.key)) {
        return C.JS_KEYCODE_SHARP_S;
    } else if (['+', '*', '~'].includes(e.key)) {
        return C.JS_KEYCODE_PLUS;
    } else if (['#', "'"].includes(e.key)) {
        return C.JS_KEYCODE_HASH;
    } else if (['-', '_'].includes(e.key)) {
        return C.JS_KEYCODE_DASH;
    } else if (['<', '>', '|'].includes(e.key)) {
        return C.JS_KEYCODE_ANGLE_BRACKET_L;
    }
    return e.keyCode || e.which;
}

function isPrintableKey(e) {
    var c = getKeycode(e);
    return C.PRINTABLE_KEYCODES.includes(c);
}

function isSpecialKey(e) {
    var c = getKeycode(e);
    return C.SPECIAL_KEYCODES.includes(c);
}

/**
 * parses the given eventList and returns the parsed text
 * @param eventList
 * @return the text that is specified by the given eventList, or null if it contains keypress events that do not produce a text
 */
function getText(eventList) {
    var text = '';
    for(var i=0; i<eventList.length; i++) {
        var elm = eventList[i];
        var code = getKeycode(elm);
        if(isSpecialKey(elm)) {
            var isAltGr = isAltGrLetter(elm, eventList[i+1], eventList[i+2]);
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
    return getKeycode(e1) == C.JS_KEYCODE_CTRL && getKeycode(e2) == C.JS_KEYCODE_ALT && isPrintableKey(e3) && e3.ctrlKey && e3.altKey;
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
            var code = C.KEYCODE_MAPPING[getKeycode(e)];
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
    return L.translate(atCmd.substring(0, C.LENGTH_ATCMD_PREFIX-1), atCmd.substring(C.LENGTH_ATCMD_PREFIX));
}