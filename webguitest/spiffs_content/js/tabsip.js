window.tabSip = {};
window.tabSip.sipSliderChanged = function (input) {
    var old = flip.getConfig([input.id]);
    var new1 = parseInt(input.value);
    var cur = flip.getLiveData([flip.LIVE_PRESSURE]);

    //only move slider if sip thresholds are below and puff thresholds are above the current live value and if strong-values are below/above normal values
    if ((input.id == flip.SIP_THRESHOLD && new1 < cur && new1 > flip.getConfig(flip.SIP_STRONG_THRESHOLD)) ||
        (input.id == flip.SIP_STRONG_THRESHOLD && new1 < cur && new1 < flip.getConfig(flip.SIP_THRESHOLD)) ||
        (input.id == flip.PUFF_THRESHOLD && new1 > cur && new1 < flip.getConfig(flip.PUFF_STRONG_THRESHOLD)) ||
        (input.id == flip.PUFF_STRONG_THRESHOLD && new1 > cur && new1 > flip.getConfig(flip.PUFF_THRESHOLD))) {

        setSliderValue(input.id, new1, true);
        flip.setValue(input.id, new1);
    } else {
        setSliderValue(input.id, old);
    }
};

window.tabSip.sipPuffValueHandler = function (data) {
    var min = data[flip.LIVE_PRESSURE_MIN];
    var max = data[flip.LIVE_PRESSURE_MAX];
    var val = data[flip.LIVE_PRESSURE];

    var border = 10; // space that is left and right of the min/max values on the sliders
    var currentMinRange = Math.max(Math.min(min - border, flip.getConfig(flip.SIP_THRESHOLD) - border, flip.getConfig(flip.SIP_STRONG_THRESHOLD) - border), 0);
    var currentMaxRange = Math.min(Math.max(max + border, flip.getConfig(flip.PUFF_THRESHOLD) + border, flip.getConfig(flip.PUFF_STRONG_THRESHOLD) + border), 1023);
    var percent = L.getPercentage(val, currentMinRange, currentMaxRange);
    var percentMin = L.getPercentage(min, currentMinRange, currentMaxRange);
    var percentMax = L.getPercentage(max, currentMinRange, currentMaxRange);

    L('#maxValue').innerHTML = max;
    L('#minValue').innerHTML = min;
    L('#currentValue').innerHTML = val;
    L('#value-bar-slider').value = val;
    L('#value-bar-slider').min = min;
    L('#value-bar-slider').max = max;

    L('#guide-max').style = 'width: ' + percentMax + '%;';
    L('#guide-min').style = 'width: ' + percentMin + '%;';

    L('#sippuff-value-bar').style = 'width: ' + percent + '%;';
    L('#guide-current').style = 'width: ' + percent + '%;';

    //color thumbs if over/under configured value
    [flip.SIP_THRESHOLD, flip.SIP_STRONG_THRESHOLD].forEach(function (c) {
        var configValue = flip.getConfig(c);
        var configPercent = L.getPercentage(configValue, currentMinRange, currentMaxRange);
        L('#' + c + '_WRAPPER').className = (percent <= configPercent) ? 'row colored-thumb' : 'row';
    });
    [flip.PUFF_THRESHOLD, flip.PUFF_STRONG_THRESHOLD].forEach(function (c) {
        var configValue = flip.getConfig(c);
        var configPercent = L.getPercentage(configValue, currentMinRange, currentMaxRange);
        L('#' + c + '_WRAPPER').className = (percent >= configPercent) ? 'row colored-thumb' : 'row';
    });

    //set slider ranges
    flip.SIP_PUFF_IDS.forEach(function (id) {
        L(id).min = currentMinRange;
        L(id).max = currentMaxRange;
    });
};