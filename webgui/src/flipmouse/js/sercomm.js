function SerialCommunicator() {
    const Serialport = require('serialport');
    
    //source: https://stackoverflow.com/questions/42730976/read-node-serialport-stream-line-by-line
    var createInterface = require('readline').createInterface;    
    var lineReader;

    //serial port instance
    var _port;

    this.setValueHandler = function (handler) {
        _valueHandler = handler;
    };
    
    this.connect = function(serialport) {
        /*_port = new SerialPort(serialport, {
          baudRate: 115200
        })
        
        lineReader = createInterface({
          input: _port
        });*/
    };
    
    //source: https://stackoverflow.com/questions/52186691/data-stream-and-promises-node-js-with-serialport
    //source: https://stackoverflow.com/questions/42730976/read-node-serialport-stream-line-by-line
    const readAsync = () => {
      const promise = new Promise((resolve, reject) => {
        /*event.once('line', (data) => {
            lineReader.on('line', function (line) {
              resolve(line);
            });
        });*/
      });
    
      return promise;
    }
    
    this.disconnect = function() {
      if(_port) _port.close();  
    };

    this.sendData = function (value, timeout) {
        if (!value) return;
        
        //send data via serial port
        //_port.write(value);
        //return promise from readAsync
        return readAsync();
    };
}
