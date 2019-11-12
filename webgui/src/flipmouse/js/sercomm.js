function SerialCommunicator() {
    const SerialPort = require('serialport');
    const Readline = require('@serialport/parser-readline')
    const Delimiter = require('@serialport/parser-delimiter')
    const parser = new Delimiter({ delimiter: '\r\n' })

    //serial port instance
    var _port;
    var _serialport;
    //value handler for reported ADC/mouthpiece values
    var _valueHandler;
    //internal value handler function for the returned data for an AT command.
    var _internalValueFunction;

    this.setValueHandler = function (handler) {
        _valueHandler = handler;
    };
    
    this.connect = function(serialport) {
        console.log("Connecting to: " + serialport);
        _port = new SerialPort(serialport, {
          baudRate: 115200 })
	    
	    //attach parser to serial port
        _port.pipe(parser);
        //on a fully received line,
        //check if the data is a reported raw value
        //or returned data for a dedicated command (see sendData)
        parser.on('data', function(data) {
			console.log("data evt: " + data);

			if (data && data.toString().indexOf(C.LIVE_VALUE_CONSTANT) > -1) {
				if (L.isFunction(_valueHandler)) {
					_valueHandler(data.toString());
				}
				return;
			}
			
			if(_internalValueFunction) {
				_internalValueFunction(data);
			}
		});
    };
    
    this.getPorts = function() {
        if(_port) {
            var list = new Array();
            _port.list().forEach( function(port) {
                list.push(port['path'] + port['manufacturer']);
            });
            return list;
        }
    };

    
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
        //wait for a response to this command
        //(there might be a timeout for commands with no response)
        return new Promise(function(resolve) {
            var result = '';
            var timeoutHandler = setTimeout(function () {
                console.log("timeout von command: " + value);
                resolve(result);
            }, timeout);
            _internalValueFunction = function(data) {
                clearTimeout(timeoutHandler);
                result += data.toString() + "\n";
                timeoutHandler = setTimeout(function () {
                    console.log("got result: " + result);
                    resolve(result);
                    _internalValueFunction = null;
                }, 50);
            }
        });
    };
}
