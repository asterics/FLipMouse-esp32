window.tabBasic = {};
window.tabBasic.basicSliderChanged = function (evt) {
    var id = evt.currentTarget.id;
    var val = evt.currentTarget.value;
    if (id.indexOf('_X') == -1 && id.indexOf('_Y') == -1) {
        //slider for x/y synchronous was moved
        L('#' + id + '_VAL').innerHTML = val;
        setSliderValue(id + '_X', val);
        setSliderValue(id + '_Y', val);
        flip.setValue(id + '_X', val);
        flip.setValue(id + '_Y', val);
    } else {
        setSliderValue(id, val, true);
        flip.setValue(id, val);
    }
};

window.tabBasic.toggleShowXY = function (constant) {
    var xVal = flip.getConfig(constant + '_X');
    var toSingle = L.isVisible('#basic-' + constant + '-xy');
    L.toggle('#basic-' + constant + '-single', '#basic-' + constant + '-xy');
    if (toSingle) {
        flip.setValue(constant + '_Y', xVal);
        L('#' + constant + '_VAL').innerHTML = xVal;
        L('#' + constant).value = xVal;
    } else {
        setSliderValue(constant, xVal);
    }
};

window.tabBasic.cursorPosZoomReset = function () {
    flip.resetMinMaxLiveValues();
    L('#cursorPosVal').innerHTML = 110;
};

window.tabBasic.cursorPosValueHandler = function (data) {
    var x = data[flip.LIVE_MOV_X];
    var y = data[flip.LIVE_MOV_Y];
    var maxX = data[flip.LIVE_MOV_X_MAX];
    var maxY = data[flip.LIVE_MOV_Y_MAX];
    var minX = data[flip.LIVE_MOV_X_MIN];
    var minY = data[flip.LIVE_MOV_Y_MIN];
    var cursorPosVal = parseInt(L('#cursorPosVal').innerHTML);
    var deadX = flip.getConfig(flip.DEADZONE_X);
    var deadY = flip.getConfig(flip.DEADZONE_Y);
    var maxAbs = Math.max(maxX, maxY, Math.abs(minX), Math.abs(minY), cursorPosVal, Math.round(deadX*1.1), Math.round(deadY*1.1));
    var percentageX = (L.getPercentage(x, -maxAbs, maxAbs));
    var percentageY = (L.getPercentage(y, -maxAbs, maxAbs));
    var percentageDzX = (L.getPercentage(flip.getConfig(flip.DEADZONE_X), 0, maxAbs));
    var percentageDzY = (L.getPercentage(flip.getConfig(flip.DEADZONE_Y), 0, maxAbs));
    var inDeadzone = x < flip.getConfig(flip.DEADZONE_X) && x > -flip.getConfig(flip.DEADZONE_X) &&
        y < flip.getConfig(flip.DEADZONE_Y) && y > -flip.getConfig(flip.DEADZONE_Y);

    if(!L.hasFocus('#posLiveA11y') || !tabBasic.lastChangedA11yPos || new Date().getTime() - tabSip.lastChangedA11yPos > 1000) {
        tabSip.lastChangedA11yPos = new Date().getTime();
        var deadzoneText = inDeadzone ? L.translate('IN_DEADZONE') : L.translate('OUT_DEADZONE');
        L('#posLiveA11y').innerHTML = deadzoneText + ", x/y "  + x + "/" + (y*-1);
    }
    L('#cursorPos').style = 'top: ' + percentageY + '%; left: ' + percentageX + '%;';
    L('#cursorPosVal').innerHTML = maxAbs;
    L('#deadZonePos').style = 'top: ' + (100 - percentageDzY) / 2 + '%; left: ' + (100 - percentageDzX) / 2 + '%; height: ' + (percentageDzY) + '%; width: ' + (percentageDzX) + '%;';
    L('#deadZonePos').className = 'back-layer color-' + (inDeadzone ? 'lightcyan' : 'lightercyan');
    L('#orientationSign').style = 'transform: rotate(' + (flip.getConfig(flip.ORIENTATION_ANGLE)+90)%360 + 'deg);';
};