app.service('mpd', function($rootScope, $location, $http, $websocket) {
    var socket;
    var status;
    var currentSong;
    var theService = {
        openSocket: function() {
        $http.get('/api/v1/status/socket').success(function(data) {
            socket = $websocket.$new('ws://'+$location.host()+':'+data.port);
            socket.$on('$message', function(message) {
                data=JSON.parse(message);
                if (data.state!==undefined) {
                    var pqChanged=status===undefined || status.playlist!==data.playlist;
                    status=data;
                    if (pqChanged) {
                        $rootScope.$broadcast("pqChanged");
                    }
                    $rootScope.$broadcast("statusChanged");
                } else if (data.id!==undefined) {
                    currentSong=data;
                    $rootScope.$broadcast("currentSongChanged");
                }
            });
        });
    },

    getState: function() {
        return undefined===status ? 'stopped' : status.state;
    },

    getPqLength: function() {
        return undefined===status ? 0 : status.playlistLength;
    },

    getPqRepeat: function() {
        return undefined===status ? 0 : status.repeat;
    },

    getPqRandom: function() {
        return undefined===status ? 0 : status.random;
    },

    getPqSingle: function() {
        return undefined===status ? 0 : status.single;
    },

    getPqConsume: function() {
        return undefined===status ? 0 : status.consume;
    },

    getCurrentSongTitle: function() {
        return undefined===currentSong ? '' : currentSong.track;
    },

    getCurrentSongArtist: function() {
        return undefined===currentSong ? '' : currentSong.artist;
    },

    getCurrentTrackId: function() {
        return undefined===currentSong ? undefined : currentSong.id;
    }
    };
    return theService;
});
