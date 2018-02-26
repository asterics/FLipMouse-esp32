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
        L('#recordedEvents').value = '';
        document.onkeydown = function(e) {
            e = e || window.event;
            e.preventDefault();
            console.log(e);
            L('#recordedEvents').value += e.code + ' ';
        };
    } else {
        document.onkeydown = null;
    }
};