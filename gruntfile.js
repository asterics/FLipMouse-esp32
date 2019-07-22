module.exports = function (grunt) {
    var source = 'webgui/src/';
    var destination = 'webgui/minified/';

    grunt.initConfig({

        // define source files and their destinations
        uglify: {
            files: {
                expand: true,
                cwd: source,
                src: '**/*.js',
                dest: destination
            }
        },
        cssmin: {
            target: {
                files: [{
                    expand: true,
                    cwd: source,
                    src: ['**/*.css'],
                    dest: destination,
                    ext: '.css'
                }]
            }
        },
        /*htmlmin: {                                     // Task
            dist: {                                      // Target
                files: {                                   // Dictionary of files
                    'webgui/minified/index.htm': source + 'index.htm'     // 'destination': 'source'
                }
            }
        },*/
        copy: {
            main: {
                files: [{
                    cwd: source + 'flipmouse/',
                    src: ['index.htm', 'favicon.ico'],
                    dest: destination + 'flipmouse/',
                    expand: true
                },
                {
                    cwd: source + 'fabi/',
                    src: ['index.htm', 'favicon.ico'],
                    dest: destination + 'fabi/',
                    expand: true
                }]
            }
        }
    });

// load plugins
    grunt.loadNpmTasks('grunt-contrib-uglify');
    grunt.loadNpmTasks('grunt-contrib-cssmin');
    //grunt.loadNpmTasks('grunt-contrib-htmlmin');
    grunt.loadNpmTasks('grunt-contrib-copy');

// register at least this one task
    grunt.registerTask('default', [ 'uglify', 'cssmin', 'copy' ]);

};