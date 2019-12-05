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

    function isATCOM({ comName }, device = 'FABI v2.3') {
        return new Promise((resolve, reject) => {
            const port = new SerialPort(
                comName,
                {
                    baudRate: 115200,
                    parser: new SerialPort.parsers.Readline('\n')
                },
                function(error) {
                    if (!error) {
                        let found = false;
                        port.on('data', chunk => {
                            const msg = chunk.toString();
                            if (msg.startsWith(device)) {
                                found = true;
                                console.log(
                                    `Found AT COM device at ${comName}`
                                );
                                port.close();
                            } else {
                                console.error(`No ${device} at ${comName}.`);
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
            const device = 'FABI v2.3'; // Proper AT COM device name
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
                        _port = port;
                        return Promise.resolve();
                    })
                    .catch(() => {
                        // throw 'No AT command capabale COM port found.';
                        console.error('No AT command capabale COM port found.');
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
                }, 50);
            };
        });
    };
}
