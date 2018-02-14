//very lightweight replacement for jquery,
//see https://blog.garstasio.com/you-dont-need-jquery/selectors/#multiple-selectors
window.L = function (selector) {
    var selectorType = 'querySelectorAll';

    if (selector.indexOf('#') === 0) {
        selectorType = 'getElementById';
        selector = selector.substr(1, selector.length);
    }

    return document[selectorType](selector);
};

window.L.toggle = function () {
    if (!arguments || arguments.length < 1) {
        return;
    }
    for (var i = 0; i < arguments.length; i++) {
        var selector = arguments[i];
        var x = L(selector);
        if (x.style && x.style.display === "none") {
            x.style.display = "block";
        } else {
            x.style.display = "none";
        }
    }
};

window.L.isVisible = function (selector) {
    var x = L(selector);
    return !(x.style && x.style.display === "none");
};

window.L.setVisible = function (selector, visible) {
    var x = L(selector);
    if(visible == false) {
        x.style.display = "none";
    } else {
        x.style.display = "block";
    }
};

window.L.val2key = function (val, array) {
    for (var key in array) {
        if (array[key] == val) {
            return key;
        }
    }
    return false;
};

window.L.isFunction = function (functionToCheck) {
    var getType = {};
    return functionToCheck && getType.toString.call(functionToCheck) === '[object Function]';
};

window.L.getIDSelector = function (id) {
    return '#' + id;
};

window.L.getPercentage = function (value, minRange, maxRange) {
    return (Math.round(((value - minRange) / (maxRange - minRange) * 100) * 1000) / 1000)
};