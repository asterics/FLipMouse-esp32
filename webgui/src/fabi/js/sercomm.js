function SerialCommunicator() {
    const SerialPort = require('serialport');
    const Readline = require('@serialport/parser-readline')
    const Delimiter = require('@serialport/parser-delimiter')
    //const parser = new Readline()
    const parser = new Delimiter({ delimiter: '\r\n' })
    
    //source: https://stackoverflow.com/questions/42730976/read-node-serialport-stream-line-by-line
    var createInterface = require('readline').createInterface;    
    var lineReader ;

    //serial port instance
    var _port;

    this.setValueHandler = function (handler) {
        _valueHandler = handler;
    };
    
    this.connect = function(serialport) {
        console.log("Connecting to: " + serialport);
        _port = new SerialPort(serialport, {
          baudRate: 115200,
          /*parser: new SerialPort.parsers.Readline('\r')*/
        })
        
        _port.pipe(parser);
    };
    
    

    
    //source: https://stackoverflow.com/questions/52186691/data-stream-and-promises-node-js-with-serialport
    //source: https://stackoverflow.com/questions/42730976/read-node-serialport-stream-line-by-line
    /*const readAsync = () => {
        console.log("read async");
        return = new Promise(function(resolve, reject) {
          _port.on('data', function(line) {
              console.log(line);
              this._valueHandler(line).then(resolve);
              //resolve(line);
          })}, function () {
                console.log("could get data!");
                reject();
            });
    }*/
    
    this.disconnect = function() {
      if(_port) _port.close(); 
      _port = null;
    };

    this.sendData = function (value, timeout) {
        if (!value) return;
        
        if(!_port) this.connect('/dev/ttyACM0');
        //send data via serial port
        _port.write(value, function(err) {
          if (err) {
            return console.log('Error on write: ', err.message)
          }
          console.log('message written')
        })
        //add NL/CR (not needed on websockets)
        _port.write('\r\n', function(err) {
          if (err) {
            return console.log('Error on write delimiter: ', err.message)
          }
          console.log('message written')
        })
        
        var timeout = 3000;
        //this here works.
        return new Promise(function(resolve) {
            var result = '';
            var timeoutHandler = setTimeout(function () {
                console.log("timeout von command: " + value);
                resolve(result);
            }, timeout);
            parser.on('data', function(data) {
                console.log("data evt");

                if (data && data.toString().indexOf(C.LIVE_VALUE_CONSTANT) > -1) {
                    if (L.isFunction(_valueHandler)) {
                        _valueHandler(data.toString());
                    }
                    return;
                }
                
                
                resolve(data.toString());
                
                clearTimeout(timeoutHandler);
                result = data.toString();
                timeoutHandler = setTimeout(function () {
                    console.log("got result: " + result);
                    resolve(result)
                }, 200);
                
                
            });
        });
        
        //Warum geht das nicht?
        /*return new Promise(function (resolve, reject) {
                this.handleData(_port, _valueHandler, timeout).then(resolve);
            }, function () {
                reject();
            });*/
    };
}

/**
 * handles got from flipmouse over a websocket. Data containing live values are passed to valueHandler function, other data resolves
 * the returned promise.
 * @param socket
 * @param valueHandler
 * @param timeout maximum time after the promise is resolved (optional). Default 3000ms.
 * @return {Promise}
 */
function handleData(port, valueHandler, timeout) {
    timeout = timeout || 3000;
    return new Promise(function(resolve) {
        var result = '';
        var timeoutHandler = setTimeout(function () {
            console.log("timeout von command: " + value);
            resolve(result);
        }, timeout);
        parser.on('data', function(data) {
            console.log("data evt");
            if (data && data.indexOf(C.LIVE_VALUE_CONSTANT) > -1) {
                if (L.isFunction(valueHandler)) {
                    valueHandler(evt.data);
                }
                return;
            }
            clearTimeout(timeoutHandler);
            result += data;
            timeoutHandler = setTimeout(function () {
                console.log("got result: " + result);
                resolve(result)
            }, 200);
        });
    });
};
