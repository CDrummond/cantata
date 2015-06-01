'use strict';

function durationStr(dur) {
    var hours = Math.floor(dur / 3600);
    var minutes = Math.floor((dur - (hours * 3600)) / 60);
    var seconds = dur - (hours * 3600) - (minutes * 60);

    if (hours < 10 && hours>0) {
        hours = "0"+hours;
    }
    if (minutes < 10) {
        minutes = "0"+minutes;
    }
    if (seconds < 10) {
        seconds = "0"+seconds;
    }
    return hours>0 ? (hours+':'+minutes+':'+seconds) : (minutes+':'+seconds);
}

angular.module('cantata.filters', [])
.filter('duration', function () {
    return function (duration) {
        if (undefined===duration) {
            return duration;
        }
        return durationStr(duration);
    }
})
.filter('displayGenre', function () {
    return function (genre) {
        if (undefined===genre || undefined===genre.name) {
            return genre;
        }
        if ("-"===genre.name) {
            return "(Unknown)";
        }

        return genre.name;
    }
})
.filter('displayGenreDetail', function () {
    return function (genre) {
        if (undefined===genre) {
            return genre;
        }
        return "Artists: "+genre.artists;
    }
})
.filter('displayArtistDetail', function () {
    return function (artist) {
        if (undefined===artist) {
            return artist;
        }
        return "Albums: "+artist.albums;
    }
})
.filter('displayAlbumDetail', function () {
    return function (album) {
        if (undefined===album) {
            return album;
        }
        return "Tracks: "+album.tracks+" ("+durationStr(album.duration)+")";
    }
})
.filter('displayTrackDetail', function () {
    return function (track) {
        if (undefined===track) {
            return track;
        }
        return durationStr(track.duration);
    }
})
.filter('displayAlbum', function () {
    return function (album) {
        if (undefined===album) {
            return album;
        }
        if (undefined===album.year || album.year<1800) {
            return undefined===album.artist ? album.name : album.artist+" - "+album.name;
        }
        return (undefined===album.artist ? album.name : album.artist+" - "+album.name)+' ('+album.year+')';
    }
})
.filter('displayTrack', function () {
    return function (track) {
        if (undefined===track) {
            return track;
        }
        if (undefined===track.track || track.track<1) {
            return track.name;
        }
        if (track.track<10) {
            return '0'+track.track+' '+track.name;
        }

        return track.track+' '+track.name;
    }
})
.filter('displayPlaylistDetail', function () {
    return function (pl) {
        if (undefined===pl) {
            return pl;
        }
        console.log(pl);
        return "Tracks: "+pl.count+" ("+durationStr(pl.duration)+")";
    }
})
;
