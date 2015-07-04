'use strict';

var app = angular.module('cantata.controllers', []);

app.controller('NavbarController', ['$scope', '$rootScope', '$location', '$http', '$compile', '$window', 'locker', 'mpd',
    function ($scope, $rootScope, $location, $http, $compile, $window, locker, mpd) {
        $scope.width = $window.innerWidth;
        $scope.page=undefined;

        $scope.$on('$routeChangeSuccess', function () {
            $scope.page=$location.path();
        });

        locker.bind($rootScope, 'libraryGrouping', 'artists');
        $scope.libraryGroupingOptions = [{name: 'Genres', value: 'genres'}, {name: 'Artists', value: 'artists'}, {name: 'Albums', value: 'albums'}];
        locker.bind($rootScope, 'artistAlbumSort', 'year');
        $scope.artistAlbumSortOptions = [{name: 'Year', value: 'year'}, {name: 'Name', value: 'name'}];
        locker.bind($rootScope, 'albumSort', 'name');
        $scope.albumSortOptions = [{name: 'Name', value: 'name'}, {name: 'Artist', value: 'artist'},
                                   {name: 'Year', value: 'year'}, {name: 'Modified Date', value: 'modified'}];

        $scope.configure = function() {
            var temp = '<form novalidate="novalidate"" style="margin:1em">' +
                       '<div class="form-group">' +
                       '<label for="libraryGrouping" class="control-label">Group library by:</label>' +
                       '<select id="libraryGrouping" class="form-control" ng-model="$root.libraryGrouping" ng-options="item.value as item.name for item in libraryGroupingOptions">' +
                       '<option ng-repeat="o in libraryGroupingOptions" value="{{o.value}}">{{o.name}}</option>' +
                       '</select>' +
                       '</div>' +

                       '<div ng-if="libraryGrouping!==\'albums\'" class="form-group">' +
                       '<label for="artistAlbumSort" class="control-label">Group albums by:</label>' +
                       '<select id="artistAlbumSort" class="form-control" ng-model="$root.artistAlbumSort" ng-options="item.value as item.name for item in artistAlbumSortOptions">' +
                       '<option ng-repeat="o in artistAlbumSortOptions" value="{{o.value}}">{{o.name}}</option>' +
                       '</select>' +
                       '</div>' +

                       '<div ng-if="libraryGrouping===\'albums\'" class="form-group">' +
                       '<label for="albumSort" class="control-label">Group albums by:</label>' +
                       '<select id="albumSort" class="form-control" ng-model="$root.albumSort" ng-options="item.value as item.name for item in albumSortOptions">' +
                       '<option ng-repeat="o in albumSortOptions" value="{{o.value}}">{{o.name}}</option>' +
                       '</select>' +
                       '</div>' +

                       '</form>';
            var linkFn = $compile(temp);
            var html= linkFn($scope);
            bootbox.dialog({title: 'Configure Cantata',
                            message: html,
                                  buttons: {
                                       success: {
                                           label: 'Close',
                                           className: 'btn-primary'
                                       }
                                   }
                            });
        };

        $scope.addStream = function() {
            bootbox.alert("Add stream not implemented!");
        };

        $scope.searchForStreams = function() {
            bootbox.alert("Stream search not implemented!");
        };

        var toggle=function(i) {
            return i===1 || i===true ? 'false' : 'true';
        }

        $scope.playQueueRepeat = function() {
            $http.post('/api/v1/status?repeat='+toggle($scope.pqRepeat));
        };

        $scope.playQueueSingle = function() {
            $http.post('/api/v1/status?single='+toggle($scope.pqSingle));
        };

        $scope.playQueueRandom = function() {
            $http.post('/api/v1/status?random='+toggle($scope.pqRandom));
        };

        $scope.playQueueConsume = function() {
            $http.post('/api/v1/status?consume='+toggle($scope.pqConsume));
        };

        $scope.playQueueClear = function() {
            console.log("CLEAR");
            bootbox.confirm("Remove all items from play queue?", function(result) {
                if (result) {
                    $http.delete('/api/v1/playqueue');
                }
            });
        };

        $scope.playQueueSave = function() {
            bootbox.alert("Play queue save not implemented!");
        };

        var update = function() {
            $scope.pqRepeat = mpd.getPqRepeat();
            $scope.pqRandom = mpd.getPqRandom();
            $scope.pqSingle = mpd.getPqSingle();
            $scope.pqConsume = mpd.getPqConsume();
        }
        update();
        $rootScope.$on("statusChanged", function() { update(); $scope.$apply(); });
}]);

app.controller('LibraryController', ['$scope', '$rootScope', '$http', 'locker',
    function ($scope, $rootScope, $http, locker) {
        $scope.level=undefined;
        $scope.genre=undefined;
        $scope.artist=undefined;
        $scope.album=undefined;
        $scope.title=undefined;
        $scope.data={};
        $scope.genres=undefined;
        $scope.artists=undefined;
        $scope.albums=undefined;

        $rootScope.$watch('libraryGrouping', function(newValue, oldValue) {
            if (newValue!==oldValue) {
                showItems();
            }
        });

        $rootScope.$watch('artistAlbumSort', function(newValue, oldValue) {
            if (newValue!==oldValue && $rootScope.libraryGrouping==='artists') {
                showItems();
            }
        });

        $rootScope.$watch('albumSort', function(newValue, oldValue) {
            if (newValue!==oldValue && $rootScope.libraryGrouping==='albums') {
                showItems();
            }
        });

        var buildParams=function(sort, play, g, ar, al, tr) {
            var params = { };
            if (undefined!==play && play) {
                params['play']=true;
            }
            if (undefined!==g) {
                params['genre']=encodeURIComponent(g.name);
            }
            if (undefined!==ar) {
                params['artistId']=encodeURIComponent(ar.id);
            } else if (undefined!==al && undefined!==al.artist) {
                params['artistId']=encodeURIComponent(al.artist);
            }
            if (undefined!==al) {
                params['albumId']=encodeURIComponent(al.id);
            }
            if (undefined!==tr) {
                params['url']=encodeURIComponent(tr.url);
            }
            if (undefined!==sort) {
                params['sort']=sort;
            }
            return params==={} ? {} : {params: params};
        }

        var sortOrder=function() {
            return 'albums'===$rootScope.libraryGrouping ? $rootScope.albumSort : $rootScope.artistAlbumSort;
        }

        $scope.showGenres = function(g) {
            if ($scope.genres!==undefined && 'artists'===$scope.level) {
                $scope.level='genres';
                $scope.data=$scope.genres;
                return;
            }
            $http.get('/api/v1/library/genres').success(function(data) {
                $scope.level='genres';
                $scope.data=$scope.genres=data;
                $scope.artists=$scope.albums=undefined;
            });
        }

        $scope.showArtists = function(g) {
            if ($scope.artists!==undefined && 'albums'===$scope.level) {
                $scope.level='artists';
                $scope.data=$scope.artists;
                return;
            }
            $http.get('/api/v1/library/artists', buildParams(undefined, undefined, g)).success(function(data) {
                $scope.level='artists';
                $scope.genre=g;
                $scope.data=$scope.artists=data;
                $scope.albums=undefined;
            });
        }

        $scope.showAlbums = function(ar) {
            if ($scope.albums!==undefined && 'tracks'===$scope.level) {
                $scope.level='albums';
                $scope.data=$scope.albums;
                return;
            }
            $http.get('/api/v1/library/albums', buildParams(sortOrder(), undefined, $scope.genre, ar)).success(function(data) {
                $scope.level='albums';
                $scope.artist=ar;
                $scope.data=$scope.albums=data;
            });
        };

        $scope.showTracks = function(al) {
            $http.get('/api/v1/library/tracks', buildParams(undefined, undefined, $scope.genre, $scope.artist, al)).success(function(data) {
                $scope.level='tracks';
                $scope.album=al;
                $scope.data=data;
            });
        };

        var showItems=function() {
            $scope.level=undefined;
            $scope.genre=undefined;
            $scope.artist=undefined;
            $scope.album=undefined;
            $scope.title=undefined;
            $scope.data={};
            $scope.genres=undefined;
            $scope.artists=undefined;
            $scope.albums=undefined;
            if ($rootScope.libraryGrouping==='genres') {
                $scope.showGenres();
            } else if ($rootScope.libraryGrouping==='artists') {
                $scope.showArtists(undefined);
            } else {
                $scope.showAlbums(undefined);
            }
        }

        showItems();

        $scope.back = function() {
            if ($rootScope.libraryGrouping==='albums') {
                if ($scope.level==='tracks') {
                    $scope.showAlbums($scope.artist);
                }
            } else {
                if ($scope.level==='tracks') {
                    $scope.showAlbums($scope.artist);
                } else if ($scope.level==='albums') {
                    $scope.artist=undefined;
                    $scope.showArtists($scope.genre);
                } else if ($scope.level==='artists') {
                    $scope.showGenres();
                }
            }
        }

        $scope.command = function(cmd, item, event) {
            if ($scope.level==='genres') {
                $http.post('/api/v1/playqueue',undefined,  buildParams(sortOrder(), 'play'===cmd, item));
            } else if ($scope.level==='artists') {
                $http.post('/api/v1/playqueue', undefined, buildParams(sortOrder(), 'play'===cmd, $scope.genre, item));
            } else if ($scope.level==='albums') {
                $http.post('/api/v1/playqueue', undefined, buildParams(sortOrder(), 'play'===cmd, $scope.genre, $scope.artist, item));
            } else if ($scope.level==='tracks') {
                $http.post('/api/v1/playqueue', undefined, buildParams(sortOrder(), 'play'===cmd, undefined, undefined, undefined, item));
            }

            if (event){
                event.stopPropagation();
                event.preventDefault();
            }
        }

        $scope.showMenu = function(item)  {
            var title='artists'===$scope.level ? item.id : item.name;
            bootbox.dialog({
                        closeButton: false,
                        title: title,
                        message: '<div><ul><li><a class="menuitem">Add</li><li><a class="menuitem">Add, and replace</li></ul></div>',
                        buttons: {
                            close:{
                                label: 'Cancel',
                            }
                        }
                    });
        }
    }
]);

app.controller('StreamsController', ['$scope', '$http',
    function ($scope, $http) {
        $scope.data={};

        var indexOf=function(stream) {
            for (var i=0; i<$scope.data.streams.length; ++i) {
                if ($scope.data.streams[i]===stream) {
                    return i;
                }
            }
            return -1;
        }

        $http.get('/api/v1/streams').success(function(data) {
            $scope.data=data;
        });

        $scope.command = function(cmd, item, event) {
            if ('del'===cmd) {
                bootbox.confirm('Are you sure you wish to delete <b>'+item.name+'</b>?', function(result) {
                    if (result) {
                        var idx=indexOf(item);
                        if (-1===idx) {
                            console.error('Could not determine index of', item);
                            return;
                        }

                        $http.delete('/api/v1/streams', {params: {index: idx}}).success(function(data) {
                            $http.get('/api/v1/streams').success(function(data) {
                                $scope.data=data;
                            });
                        });
                    }
                });
            } else if ('play'===cmd) {
                $http.post('/api/v1/playqueue', undefined, { params: { url: item.url, stream: true, play: true }} );
            }

            if (event){
                event.stopPropagation();
                event.preventDefault();
            }
        }
    }
]);

app.controller('PlayListsController', ['$scope', '$http',
    function ($scope, $http) {
        $scope.level=undefined;
        $scope.data={};
        $scope.playlists=undefined;

        var indexOf=function(track) {
            for (var i=0; i<$scope.data.tracks.length; ++i) {
                if ($scope.data.tracks[i]===track) {
                    return i;
                }
            }
            return -1;
        }

        $scope.showPlaylists = function() {
            if ($scope.playlists!==undefined) {
                $scope.level='playlists';
                $scope.data=$scope.playlists;
                return;
            }
            $http.get('/api/v1/playlists').success(function(data) {
                $scope.level='playlists';
                $scope.data=$scope.playlists=data;
            });
        }

        $scope.showTracks = function(playlist) {
            $http.get('/api/v1/playlists/tracks', {params: { name: playlist}} ).success(function(data) {
                $scope.level='tracks';
                $scope.playlist=playlist;
                $scope.data=data;
            });
        };

        $scope.command = function(cmd, item, event) {
            if ('del'===cmd) {
                if ($scope.level==='tracks') {
                    var idx=indexOf(item);
                    if (-1===idx) {
                        console.error('Could not determine index of', item);
                        return;
                    }
                    $http.delete('/api/v1/playlists/'+$scope.playlist, {params: {index: idx}}).success(function(data) {
                        $scope.showTracks($scope.playlist);
                    });
                } else {
                    bootbox.confirm('Are you sure you wish to delete <b>'+item+'</b>?', function(result) {
                        if (result) {
                            $http.delete('/api/v1/playlists', {params: {name: item}}).success(function(data) {
                                $http.get('/api/v1/playlists').success(function(data) {
                                    $scope.data=$scope.playlists=data;
                                });
                            });
                        }
                    });
                }
            } else if ('add'===cmd || 'play'===cmd) {
                if ($scope.level==='tracks') {
                    $http.post('/api/v1/playqueue', undefined, { params: { url: item.file, play: 'play'===cmd }} );
                } else {
                    $http.post('/api/v1/playqueue', undefined, { params: { url: item.name, playlist: true, play: 'play'===cmd }} );
                }
            }

            if (event){
                event.stopPropagation();
                event.preventDefault();
            }
        }

        $scope.back = function() {
            if ($scope.level==='tracks') {
                $scope.showPlaylists();
            }
        }

        $scope.showPlaylists();
    }
]);

app.controller('PlayQueueController', ['$scope', '$rootScope', '$http', 'mpd',
    function ($scope, $rootScope, $http, mpd) {
        $scope.data={};
        $scope.currentTrack=mpd.getCurrentTrackId();

        $scope.update = function() {
            $http.get('/api/v1/playqueue').success(function(data) {
                $scope.data=data;
            });
        }

        $scope.update();
        $rootScope.$on("pqChanged", function() { $scope.update(); $scope.$apply(); });
        $rootScope.$on("currentSongChanged", function() { $scope.currentTrack=mpd.getCurrentTrackId(); $scope.$apply(); });

        $scope.showMenu = function(item) {
        }
    }
]);

app.controller('MiniPlayerController', ['$scope', '$rootScope', '$http', '$window', 'mpd',
    function ($scope, $rootScope, $http, $window, mpd) {
        $scope.width = $window.innerWidth;
        var update = function() {
            $scope.currentSongTitle = mpd.getCurrentSongTitle();
            $scope.currentSongArtist = mpd.getCurrentSongArtist();
            $scope.state = mpd.getState();
            $scope.pqLength = mpd.getPqLength();
        }
        mpd.openSocket();
        update();
        $rootScope.$on("currentSongChanged", function() { update(); $scope.$apply(); });
        $rootScope.$on("statusChanged", function() { update(); $scope.$apply(); });

        $scope.prevTrack = function() {
            if (undefined!==$scope.pqLength && $scope.pqLength>1) {
                $http.post('/api/v1/playqueue/control?cmd=prev');
            }
        }
        $scope.play = function() {
            if (undefined!==$scope.pqLength && $scope.pqLength>0 && undefined!==$scope.state && 'playing'!==$scope.state) {
                $http.post('/api/v1/playqueue/control?cmd=play');
            }
        }
        $scope.pause = function() {
            if (undefined!==$scope.pqLength && $scope.pqLength>0 && undefined!==$scope.state && 'playing'===$scope.state) {
                $http.post('/api/v1/playqueue/control?cmd=pause');
            }
        }
        $scope.nextTrack = function() {
            if (undefined!==$scope.pqLength && $scope.pqLength>1) {
                $http.post('/api/v1/playqueue/control?cmd=next');
            }
        }
    }
]);
