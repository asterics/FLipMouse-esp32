window.tabGlobal = {};

tabGlobal.showCheckmark = function (btn) {
    L.setVisible(btn.children[1], true, 'inline');
    setTimeout(function () {
        L.setVisible(btn.children[1], false);
    }, 3000);
};