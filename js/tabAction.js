window.tabAction = {};
window.tabAction.initBtnModeActionTable = function () {
    L.removeAllChildren('#currentConfigTb');
    flip.BTN_MODES.forEach(function (btnMode) {
        var liElm = L.createElement('li', 'row');
        var currentActionDiv = L.createElement('div', 'eight columns', L.createElement('span', '', flip.getConfig(btnMode) + ' '));
        var changeA = L.createElement('a', '', 'change');
        changeA.href = 'javascript:tabAction.selectActionButton("' + btnMode + '")';
        currentActionDiv.appendChild(changeA);
        liElm.appendChild(L.createElement('div', 'two columns', btnMode));
        liElm.appendChild(currentActionDiv);
        L('#currentConfigTb').appendChild(liElm);
    });
};

window.tabAction.initCombos = function () {
    L.removeAllChildren('#selectActionButton');
    flip.BTN_MODES.forEach(function (btnMode) {
        var option = L.createElement('option', '', btnMode);
        option.value = btnMode;
        L('#selectActionButton').appendChild(option);
    });
    L('#selectActionButton').value = '';
    flip.LEARN_CATEGORIES.forEach(function (cat) {
        var option = L.createElement('option', '', cat);
        option.value = cat;
        L('#selectActionCategory').appendChild(option);
    });
    L('#selectActionCategory').value = '';
};

window.tabAction.selectActionButton = function (btnMode) {
    console.log(btnMode);
    L('#selectActionButton').value = btnMode;
};

window.tabAction.startRec = function () {
    if(!document.onkeydown) {
        L('#recordedAction').innerHTML = '';
        L('#recordedAtCmd').innerHTML = '';
        window.tabAction.queue = [];
        document.onkeydown = function(e) {
            e = e || window.event;
            e.preventDefault();
            var last = L.getLastElement(window.tabAction.queue);
            if(!e.repeat && C.SUPPORTED_KEYCODES.includes(getKeycode(e))) {
                if(getKeycode(e) != C.JS_KEYCODE_BACKSPACE || !getText(tabAction.queue)) {
                    tabAction.queue.push(e);
                } else {
                    tabAction.queue.pop();
                    if(!getText(tabAction.queue)) {
                        tabAction.queue = [];
                    }
                }
                console.log(e);
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
    if(atCmd.indexOf(C.AT_CMD_WRITEWORD) > -1) {
        return "Write word: " + "'" + atCmd.substring(C.LENGTH_ATCMD_PREFIX) + "'";
    } else if(atCmd.indexOf(C.AT_CMD_KEYPRESS) > -1) {
        return "Press keys: " + L.replaceAll(atCmd.substring(C.LENGTH_ATCMD_PREFIX), ' ', ' + ');
    }
    return '';
}