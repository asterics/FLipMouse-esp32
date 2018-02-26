window.tabAction = {};
window.tabAction.initBtnModeActionTable = function () {
    L.removeAllChildren('#currentConfigTb');
    flip.BTN_MODES.forEach(function (btnMode) {
        var liElm = L.createElement('li', 'row');
        var currentActionDiv = L.createElement('div', 'eight columns', L.createElement('span', '', flip.getConfig(btnMode) + ' '));
        var changeA = L.createElement('a', '', 'change');
        changeA.href = 'javascript:tabAction.change("' + btnMode + '")';
        currentActionDiv.appendChild(changeA);
        liElm.appendChild(L.createElement('div', 'two columns', btnMode));
        liElm.appendChild(currentActionDiv);
        L('#currentConfigTb').appendChild(liElm);
        /*
        <li class="row">
                <div class="two columns">Stick UP</div>
                <div class="two columns">AX 100</div>
                <div class="two columns"><a href="#">change</a></div>
            </li>
         */
    });
};

window.tabAction.change = function (btnMode) {
    console.log(btnMode);
};