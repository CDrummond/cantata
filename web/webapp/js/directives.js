'use strict';

var app = angular.module('cantata.directives', []);

app.directive('watchResize', function(){
    return {
        restrict: 'A',
        link: function(scope, elem, attr) {
            angular.element(window).on('resize', function(){
                scope.$apply(function(){
                    scope.width=window.innerWidth;
                });
            });
        }
    }
});

app.directive('onLongPress', function($timeout) {
    return {
        restrict: 'A',
        link: function($scope, $elm, $attrs) {
            $elm.bind('touchstart', function(evt) {
                // Locally scoped variable that will keep track of the long press
                $scope.longPress = true;

                // We'll set a timeout for 600 ms for a long press
                $timeout(function() {
                    if ($scope.longPress) {
                        // If the touchend event hasn't fired,
                        // apply the function given in on the element's on-long-press attribute
                        $scope.$apply(function() {
                            $scope.$eval($attrs.onLongPress)
                        });
                    }
                }, 750);
            });

            $elm.bind('touchend', function(evt) {
                // Prevent the onLongPress event from firing
                $scope.longPress = false;
                // If there is an on-touch-end function attached to this element, apply it
                if ($attrs.onTouchEnd) {
                    $scope.$apply(function() {
                        $scope.$eval($attrs.onTouchEnd)
                    });
                }
            });

            $elm.bind('touchmove', function(evt) {
                // Prevent the onLongPress event from firing
                $scope.longPress = false;
                // If there is an on-touch-move function attached to this element, apply it
                if ($attrs.onTouchMove) {
                    $scope.$apply(function() {
                        $scope.$eval($attrs.onTouchMove)
                    });
                }
            });
        }
    };
})
