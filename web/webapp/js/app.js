'use strict';

angular.module('cantata', ['ngRoute', 'ngSanitize', 'ngWebsocket', 'angular-locker', 'me-lazyload', 'cantata.filters', 'cantata.controllers', 'cantata.directives']).
    config(['$routeProvider', function ($routeProvider) {
        $routeProvider.when('/library', {templateUrl: 'partials/library.html', controller: 'LibraryController'});
        $routeProvider.when('/streams', {templateUrl: 'partials/streams.html', controller: 'StreamsController'});
        $routeProvider.when('/playlists', {templateUrl: 'partials/playlists.html', controller: 'PlayListsController'});
        $routeProvider.when('/playqueue', {templateUrl: 'partials/playqueue.html', controller: 'PlayQueueController'});
        $routeProvider.otherwise({redirectTo: '/library'});
    }],
    function(lockerProvider) {
        lockerProvider.defaults({ namespace: 'Cantata', driver: 'local' });
    }
);
