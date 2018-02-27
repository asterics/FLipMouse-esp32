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
        var elems = L(selector);
        if(elems.length) {
            elems.forEach(function (x) {
                toggle(x);
            });
        } else if(elems.style) {
            toggle(elems);
        }

        function toggle(x) {
            if (x.style && x.style.display === "none") {
                x.style.display = "block";
            } else {
                x.style.display = "none";
            }
        }
    }
};

window.L.isVisible = function (selector) {
    var x = L(selector);
    return !(x.style && x.style.display === "none");
};

window.L.setVisible = function (selector, visible) {
    var elems = L(selector);
    if(elems.length) {
        elems.forEach(function (x) {
            setVisible(x);
        });
    } else if(elems.style) {
        setVisible(elems);
    }

    function setVisible(x) {
        if(visible == false) {
            x.style.display = "none";
        } else {
            x.style.display = "block";
        }
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

window.L.getMs = function () {
    return new Date().getTime();
};

window.L.deepCopy = function (object) {
    return JSON.parse(JSON.stringify(object));
};

window.L.removeAllChildren = function (selector) {
    var elm = L(selector);
    elm = elm instanceof NodeList ? elm : [elm];
    elm.forEach(function (elem) {
        while (elem.firstChild) {
            elem.removeChild(elem.firstChild);
        }
    });
};

window.L.createElement = function(tagName, className, inner) {
    var e = document.createElement(tagName);
    e.className = className;
    if(inner && typeof inner === 'string') {
        e.innerHTML = inner;
    } else if(inner) {
        e.appendChild(inner);
    }
    return e;
};

/**
 * returns true if the current browser language contains the given localeString
 */
window.L.isLang = function (localeString) {
    var lang = window.navigator.userLanguage || window.navigator.language;
    return lang.indexOf(localeString) > -1;
};

window.L.translate = function(enString, deString) {
    return L.isLang('de') ? deString : enString;
};

window.L.getLastElement = function(array) {
    return array.slice(-1)[0];
};

window.L.replaceAll = function(string, search, replace) {
    return string.replace(new RegExp(search, 'g'), replace);
};