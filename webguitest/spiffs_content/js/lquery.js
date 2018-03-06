//very lightweight replacement for jquery,
//see https://blog.garstasio.com/you-dont-need-jquery/selectors/#multiple-selectors
window.L = function (selector) {
    if(selector instanceof Node) {
        return selector;
    }
    var selectorType = 'querySelectorAll';

    if (selector.indexOf('#') === 0) {
        selectorType = 'getElementById';
        selector = selector.substr(1, selector.length);
    }

    return document[selectorType](selector);
};

window.L.toggle = function () {
    var mainArguments = Array.prototype.slice.call(arguments);
    mainArguments.unshift("block");
    toggleInternal(mainArguments);
};

window.L.toggleInline = function () {
    var mainArguments = Array.prototype.slice.call(arguments);
    mainArguments.unshift("inline");
    toggleInternal(mainArguments);
};

function toggleInternal(arguments) {
    var displayModeShown = arguments[0];
    if (!arguments || arguments.length < 2) {
        return;
    }
    for (var i = 1; i < arguments.length; i++) {
        var selector = arguments[i];
        var elems = L.selectAsList(selector);
        elems.forEach(function (x) {
            if (x.style && x.style.display === "none") {
                x.style.display = displayModeShown;
            } else {
                x.style.display = "none";
            }
        });
    }
}

window.L.isVisible = function (selector) {
    var x = L(selector);
    return !(x.style && x.style.display === "none");
};

window.L.setVisible = function (selector, visible) {
    var elems = L.selectAsList(selector);
    elems.forEach(function (x) {
        if(visible == false) {
            x.style.display = "none";
        } else {
            x.style.display = "block";
        }
    });
};

window.L.selectAsList = function (selector) {
    var result = L(selector);
    if(result && result.length > 0) {
        return result;
    }
    return result && !(result instanceof NodeList) ? [result]: [];
};

window.L.addClass = function (selector, className) {
    var list = L.selectAsList(selector);
    list.forEach(function (elem) {
        elem.className += ' ' + className;
    });
};

window.L.removeClass = function (selector, className) {
    var list = L.selectAsList(selector);
    list.forEach(function (elem) {
        elem.className = L.replaceAll(elem.className, className, '');
    });
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

window.L.getLang = function () {
    var lang = window.navigator.userLanguage || window.navigator.language;
    return lang.substring(0,2);
};

/**
 * translates an translation key. More arguments can be passed in order to replace placeholders ("{?}") in the translated texts.
 * e.g.
 * var key = 'SAY_HELLO_KEY'
 * translation: 'SAY_HELLO_KEY' -> 'Hello {?} {?}'
 * L.translate(key, 'Tom', 'Mayer') == 'Hello Tom Mayer'
 *
 * @param translationKey the key to translate
 * @return {*}
 */
window.L.translate = function(translationKey) {
    var translated = i18n[translationKey] ? i18n[translationKey] : translationKey;
    for(var i=1; i<arguments.length; i++) {
        translated = translated.replace('{?}', arguments[i]);
    }
    return translated;
};

window.L.getLastElement = function(array) {
    return array.slice(-1)[0];
};

window.L.replaceAll = function(string, search, replace) {
    return string.replace(new RegExp(search, 'g'), replace);
};

window.L.equalIgnoreCase = function (str1, str2) {
    return str1.toUpperCase() === str2.toUpperCase();
};

window.L.loadScript = function (source, fallbackSource) {
    console.log("loading script: " + source);
    var script = document.createElement('script');
    return new Promise(function (resolve) {
        script.onload = function () {
            console.log("loaded: " + source);
            resolve(true);
        };
        script.onerror = function () {
            console.log("error loading: " + source);
            if(fallbackSource) {
                L.loadScript(fallbackSource).then(resolve);
            } else {
                resolve(false);
            }
        };
        script.src = source;
        document.head.appendChild(script);
    });
};