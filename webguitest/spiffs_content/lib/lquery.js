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

window.L.toggle = function (selector, selector2) {
    if (!selector) {
        return;
    }
    var x = L(selector);
    if (x.style && x.style.display === "none") {
        x.style.display = "block";
    } else {
        x.style.display = "none";
    }
    L.toggle(selector2);
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