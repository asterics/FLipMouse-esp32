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