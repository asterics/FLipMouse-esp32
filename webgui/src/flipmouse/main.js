//according to:
//https://electronjs.org/docs/tutorial/first-app

const { app, BrowserWindow } = require('electron')

function createWindow () {
  // Create the browser window.
  let win = new BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: {
      nodeIntegration: true,
      toolbar: false
    }
  })

  // and load the index.html of the app.
  win.loadFile('index.htm')
  win.setMenuBarVisibility(false);
  win.setAutoHideMenuBar(true);
}

app.on('ready', createWindow) 
