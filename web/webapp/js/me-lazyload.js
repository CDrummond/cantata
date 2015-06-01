/**
 * me-lazyload
 *
 * Images lazyload directive for angular.
 *
 * Licensed under the MIT license.
 * Copyright 2014 Treri Liu
 * https://github.com/Treri/me-lazyload
 */

 angular.module('me-lazyload', [])
.directive('lazySrc', ['$window', '$document', '$interval', '$timeout', function($window, $document, $interval, $timeout){
    var doc = $document[0],
        body = doc.body,
        win = $window,
        $win = angular.element(win),
        uid = 0,
        elements = {},
        interval = undefined,
        boundEvents = false;

    function getUid(el){
        return el.__uid || (el.__uid = '' + ++uid);
    }

    function getWindowOffset(){
        var t,
            pageXOffset = (typeof win.pageXOffset == 'number') ? win.pageXOffset : (((t = doc.documentElement) || (t = body.parentNode)) && typeof t.ScrollLeft == 'number' ? t : body).ScrollLeft,
            pageYOffset = (typeof win.pageYOffset == 'number') ? win.pageYOffset : (((t = doc.documentElement) || (t = body.parentNode)) && typeof t.ScrollTop == 'number' ? t : body).ScrollTop;
        return {
            offsetX: pageXOffset,
            offsetY: pageYOffset
        };
    }

    function isVisible(iElement){
        var elem = iElement[0],
            elemRect = elem.getBoundingClientRect(),
            elemWidth = elemRect.width,
            elemHeight = elemRect.height;

        if (0===elemWidth && 0===elemHeight) {
            return false;
        }

        var windowOffset = getWindowOffset(),
            winOffsetX = windowOffset.offsetX,
            winOffsetY = windowOffset.offsetY,
            elemOffsetX = elemRect.left + winOffsetX,
            elemOffsetY = elemRect.top + winOffsetY,
            viewWidth = Math.max(doc.documentElement.clientWidth, win.innerWidth || 0),
            viewHeight = Math.max(doc.documentElement.clientHeight, win.innerHeight || 0),
            xVisible,
            yVisible;

        if(elemOffsetY <= winOffsetY){
            if(elemOffsetY + elemHeight >= winOffsetY){
                yVisible = true;
            }
        }else if(elemOffsetY >= winOffsetY){
            if(elemOffsetY <= winOffsetY + viewHeight){
                yVisible = true;
            }
        }

        if(elemOffsetX <= winOffsetX){
            if(elemOffsetX + elemWidth >= winOffsetX){
                xVisible = true;
            }
        }else if(elemOffsetX >= winOffsetX){
            if(elemOffsetX <= winOffsetX + viewWidth){
                xVisible = true;
            }
        }

        return xVisible && yVisible;
    };

    function checkImage(){
        var updated=[];
        Object.keys(elements).forEach(function(key){
            var obj = elements[key],
                iElement = obj.iElement,
                $scope = obj.$scope;

            if(isVisible(iElement) && iElement.attr('src')!==$scope.lazySrc){
                iElement.attr('src', $scope.lazySrc);
                updated.push(key);
            }
        });
        for (var i=0; i<updated.length; ++i) {
            delete elements[updated[i]];
        }
        if (0===Object.keys(elements).length) {
            // No unset images left, so cancel timer...
            stopTimer();
        }
    }

    // Can't seem to detect div being resized, so for this scenario we check every second...
    function startTimer() {
        if (undefined===interval) {
            interval=$interval(function () { checkImage(); }, 1000);
        }
    }

    function stopTimer() {
        if (undefined!==interval) {
            $interval.cancel(interval);
            interval=undefined;
        }
    }

    function onLoad(){
        var $el = angular.element(this),
            uid = getUid($el);

        $el.css('opacity', 1);

        if(elements.hasOwnProperty(uid)){
            delete elements[uid];
        }
    }

    return {
        restrict: 'A',
        scope: {
            lazySrc: '@'
        },
        link: function($scope, iElement){
            iElement.bind('load', onLoad);

            $scope.$watch('lazySrc', function(){
                if(isVisible(iElement)){
                    if (iElement.attr('src')!==$scope.lazySrc) {
                        iElement.attr('src', $scope.lazySrc);
                    }
                }else{
                    var uid = getUid(iElement);
                    iElement.css({
                        'background-color': 'rgba(1, 1, 1, 0)',
                        'opacity': 0,
                        '-webkit-transition': 'opacity 0.25s',
                        'transition': 'opacity 0.25s'
                    });
                    elements[uid] = {
                        iElement: iElement,
                        $scope: $scope
                    };
                    startTimer();
                }

                if (!boundEvents) {
                    $win.bind('scroll', checkImage);
                    $win.bind('resize', checkImage);
                    $(".lazy-load-scroll").bind('scroll', checkImage);
                    boundEvents=true;
                }
            });

            $scope.$on('$destroy', function(){
                iElement.unbind('load');
                elements = {};
                uid = 0;
                if (boundEvents) {
                    $win.unbind('scroll');
                    $win.unbind('resize');
                    $(".lazy-load-scroll").unbind('scroll');
                    boundEvents=false;
                }
                stopTimer();
            });
        }
    };
}]);
