
if( ( /android/gi ).test( navigator.appVersion ) || true ) {
    console = {
        "_log" : [],
        "log" : function() {
            var arr = [];
            for ( var i = 0; i < arguments.length; i++ ) {
                arr.push( arguments[ i ] );
            }
            this._log.push( arr.join( ", ") );
        },
        "warn" : function() {
            console.log(arguments);
        },
        "trace" : function() {
            var stack;
            try {
                throw new Error();
            } catch( ex ) {
                stack = ex.stack;
            }
            console.log( "console.trace()\n" + stack.split( "\n" ).slice( 2 ).join( "  \n" ) );
        },
        "dir" : function( obj ) {
            console.log( "Content of " + obj );
            for ( var key in obj ) {
                var value = typeof obj[ key ] === "function" ? "function" : obj[ key ];
                console.log( " -\"" + key + "\" -> \"" + value + "\"" );
            }
        },
        "show" : function() {
            alert( this._log.join( "\n" ) );
            this._log = [];
        }
    };
    window.onerror = function( msg, url, line ) {
        console.log("ERROR: \"" + msg + "\" at \"" + "\", line " + line + "\", url " + url);
    };
    setTimeout(function () {
        console.show();
    }, 2000);
}