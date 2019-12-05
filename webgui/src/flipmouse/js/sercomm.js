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
    
    function ping(port_name){
		try {
		   const port = new SerialPort(port_name,{
			 baudRate: 115200,
			 parser: new SerialPort.parsers.Readline('\n')
		   });
		   port.on('open',function(){
		     port.write("AT ID \r\n");
		     data = port.read();
		     //TODO: test for valid id
		     console.log(i + data);
		     port.close();
		  });
		
		  return new Promise(resolve => port.on("close", resolve));
	   } catch(err) {
		   console.log("Not a valid port");
		   return new Promise(function (resolve, reject) { 
			   reject();
		   });
	   }
	
	   
	}
	
    
    this.connect = async function() {
		 

			let portname = await this.getValidPort();
			
			serialport = "/dev/ttyACM0";
			
			
		 //})();
		
		/*(async function() {
			const portlist = await SerialPort.list();
			let serialport = "";
			for(const port of portlist) {
				//open port
				probePort = new SerialPort(port['comName'], {baudRate: 115200 })
				probePort.pipe(parser);
				
				//send data
				
				//await  for feedback
				
				// if flipmouse -> set _port & return
			}*/
			
			
			
			//is port defined?
			
			//attach parser to serial port
			_port = new SerialPort(serialport, {
				baudRate: 115200 })
			_port.pipe(parser);
			console.log("Found valid device @" + serialport);
				    
        
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
		//})();
    };
    
    this.getValidPort = async function() {
		const portlist = await SerialPort.list();
		for(let i = 0; i<portlist.length; i++) {
			await ping(portlist[i].comName);	
		}
    };

    
    this.disconnect = function() {
      if(_port) _port.close(); 
      _port = null;
    };

    this.sendData = function (value, timeout) {
		const that = this;
        (async function() {
        
			if (!value) return;
        
        
			if(!_port) await that.connect();
		
        //if still no port exists, we cannot send data...
        if(!_port) {
			console.log("no ports found");
			return new Promise(function(resolve) {
				resolve("");
			});
        }
        
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
       })();
    };
}
