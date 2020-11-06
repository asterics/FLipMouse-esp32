function SerialCommunicator() {
    const SerialPort = require('serialport');
    const Readline = require('@serialport/parser-readline');
    const Delimiter = require('@serialport/parser-delimiter');
    const parser = new Delimiter({ delimiter: '\r\n' });

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

    function isATCOM({ comName }, device = 'FLipMouse') {
        return new Promise((resolve, reject) => {
            const port = new SerialPort(
                comName,
                {
                    baudRate: 115200,
                    highWaterMark: 1024,
                    parser: new SerialPort.parsers.Readline('\n')
                },
                function(error) {
                    if (!error) {
                        let found = false;
                        port.on('data', chunk => {
                            const msg = chunk.toString();
                            if (msg.toLowerCase().startsWith(device.toLowerCase())) {
                                found = true;
                                console.log(
                                    `Found AT COM device at ${comName}:`
                                );
                                var ver = msg.replace(/(\r\n|\n|\r)/gm, "");
                                ver = ver.split(" ");
                                console.log(msg);
                                port.close();
                                flip.VERSION = ver[1];
                            } else {
                                console.error(`No ${device} at ${comName}.`);
                                port.close();
                                reject();
                            }
                        });
                        port.on('close', () => resolve(port));
                        port.write('AT ID \r\n');
                        setTimeout(() => {
                            if (!found) {
                                console.error(
                                    `Serial device ${comName} is not responding.`
                                );
                                reject();
                            }
                        }, 2000); // Reject if serial port is not responding.
                    } else {
                        console.log(error.message);
                        reject();
                    }
                }
            );
        });
    }

    this.init = function() {
        return SerialPort.list().then(function(ports, errors) {
            const device = 'FLipMouse'; // Proper AT COM device name
            console.log(`Searching for ${device} ...`);
            if (typeof errors === 'undefined') {
                // race to (first) success, cf. https://stackoverflow.com/a/37235274/5728717
                return Promise.all(
                    ports
                        .map(port => isATCOM(port, device))
                        .map(port => {
                            return port.then(
                                resolve => Promise.reject(resolve),
                                reject => Promise.resolve(reject)
                            );
                        })
                )
                    .then(
                        resolve => Promise.reject(resolve),
                        reject => Promise.resolve(reject)
                    )
                    .then(port => {
                        _port = new SerialPort(port['path'], {
                            baudRate: 115200
                        });;
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
                        return Promise.resolve();
                    })
                    .catch(() => {
                        // throw 'No AT command capable COM port found.';
                        console.error('No AT command capable COM port found.');
                    });
            } else {
                console.error(errors);
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
    
    //send raw data without line endings (used for transferring binary data, e.g. updates)
    this.sendRawData = function (value, timeout) {
		if (!value) return;
        if (!_port) {
            throw 'sercomm: port not initialized. call init() before sending data.';
        }
		//send data via serial port
        _port.write(value, function(err) {
          if (err) {
            return console.log('Error on write: ', err.message);
          }
          //console.log('message written');
        });
        console.log('finished write');
        _port.drain(function() {
			console.log('finished drain');
			flip.inRawMode = false;
		});
	}

	//send data line based (for all AT commands)
    this.sendData = function (value, timeout) {
        if (!value) return;
        if (!_port) {
            throw 'sercomm: port not initialized. call init() before sending data.';
        }

        //send data via serial port
        _port.write(value, function(err) {
          if (err) {
            return console.log('Error on write: ', err.message);
          }
          console.log('message written');
        });
        //add NL/CR (not needed on websockets)
        _port.write('\r\n', function(err) {
          if (err) {
            return console.log('Error on write delimiter: ', err.message);
          }
          console.log('message written');
        });

        var timeout = 3000;
        //wait for a response to this command
        //(there might be a timeout for commands with no response)
        return new Promise(function(resolve) {
            var result = '';
            var timeoutHandler = setTimeout(function () {
                console.log("timeout of command: " + value);
                resolve(result);
            }, timeout);
            _internalValueFunction = function(data) {
                clearTimeout(timeoutHandler);
                result += data.toString() + "\n";
                timeoutHandler = setTimeout(function () {
                    console.log("got result: " + result);
                    resolve(result);
                    _internalValueFunction = null;
                }, 200);
            };
        });
    };
}
