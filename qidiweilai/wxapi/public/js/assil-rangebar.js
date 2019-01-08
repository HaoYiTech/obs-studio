/// <reference path="jquery-1.11.3.min.js" />
/// <reference path="jquery-ui.min.js" />
/// <reference path="jquery.resize.js" />

/// <reference path="assil-rangebar.js" />

// https://github.com/alansferreira/assil-jquery-rangebar

/**
events: 
@param event inherits event from event signature of draggable:drag/resizable:resize
@param ui inherits ui from ui signature of draggable:drag/resizable:resize
@param hint overlap params
@param $bar source bar element
@param $range source range element overlaping
@param $obstacle ovelaped object
function overlap(event, ui, hint, $bar, $range, $obstacle)

@param event inherits event from event signature of draggable:drag/resizable:resize
@param ui inherits ui from ui signature of draggable:drag/resizable:resize
@param $bar source bar element
@param $range source range element overlaping
function change(event, ui, $bar, $range)
function range_click(event, ui, $bar, $range)
*/  

var g_assil = { 
  debgug: false,
  cur_max_id: 1,
};

(function ($) {
    $.widget("assil.rangebar", $.ui.mouse, {
        options: {
            defaultRange: {
                start: 3, end: 9,
                disabled: false,
                css: {
                    range: 'custom-range-green',
                    label: ''
                },
                allowDelete: true, //indicates if can ranges can be removed
                canOverlap: false,
                teacherID: 1,   // default teacherID for cloud record
                subjectID: 1,   // default subjectID for cloud record
                courseID:  0,   // default courseID for database
            },
            ranges: [],
            step: 1,
            min: 0, 
            max: 100,
            deleteTimeout: 3000,
            orientation: "horizontal",
            defSecond: 1800,

            //callback functions
            label: null,        // function to computes label display of range
            renderRange: null,  // function($range, range){} occurs on addRange
            updateRange: null,  // function($range, range){} occurs on range change, move or resize
            deleteEvent: null,  // range delete event notify...
            dbClickEvent: null, // range double click event notify...
            mouseCreate: null,  // range created by left mouse down...
        },
        _valueMin: function() {
          return this.options.min;
        },
        _valueMax: function() {
          return this.options.max;
        },
        _detectOrientation: function() {
          this.orientation = ( this.options.orientation === "vertical" ) ? "vertical" : "horizontal";
        },
        _create: function () {
            var _component = this;
            //_component._mouseInit();
            _component._detectOrientation();
            _component.setRanges(_component.options.ranges);
            _component.element.resize(function () {
                var $bar = $(_component.element);
                $.each($bar.find('.range'), function () {
                    var $range = $(this);
                    range = $range.data("range");
                    if (!range) return true;
                    _component.updateRangeUI($range);
                });

            });
            _component.element.getRanges = _component.getRanges
            _component._addClass( "ui-slider ui-slider-" + this.orientation, "ui-widget ui-widget-content" );
            
            $(_component.element).on('mousedown', this._do_mouse_down);
        },
        _destroy: function () {
            $.Widget.prototype.destroy.call(this);
        },
        _do_mouse_down: function(ev) {
          // except element, must be rangebar...
          if( !$(ev.target).hasClass('ui-slider') )
            return false;
          // get info and create new range...
          var $bar = $(this);
          var barWidth = $bar.width();
          var options = $bar.data("assil-rangebar").options;
          var totalRange = options.max - options.min;
          var nSecFrom = valueFromPercent(totalRange, percentOf(barWidth, ev.offsetX));
          var nSecTo = nSecFrom + options.defSecond; // default half hour range...
          if( nSecTo > options.max )
            return false;
          var nStartX = valueFromPercent(barWidth, percentOf(totalRange, nSecFrom));
          var nEndX = valueFromPercent(barWidth, percentOf(totalRange, nSecTo));
          var range_rect = { x: nStartX, y: 0, w: nEndX - nStartX, h: $bar.height() };
          var getRect = getRectUsing$Position;
          var siblings_rects = [];
          // find all the sibling ranges...
          $bar.children('.range').each(function () {
            siblings_rects.push(getRect(this));
          });
          // if overlaped then remove it, not overlaped increase the id...
          var overlaps = $(range_rect).overlapsX(siblings_rects);
          if( overlaps.length > 0 ) {
            return false;
          }
          // create a new range object...
          var range = { start: nSecFrom, end: nSecTo };
          var $range = $bar.rangebar('addFloatRange', range);
          // notify create a new range...
          if( options.mouseCreate ) options.mouseCreate($range);
          return true;
        },
        checkRange: function(strStart, strEnd, nTeacher, nSubject) {
          var $bar = $(this.element);
          var barWidth = $bar.width();
          var options = $bar.data("assil-rangebar").options;
          var totalRange = options.max - options.min;
          var nSecFrom = $bar.data("float-options").timeToSec(strStart);
          var nSecTo = $bar.data("float-options").timeToSec(strEnd);
          var nStartX = valueFromPercent(barWidth, percentOf(totalRange, nSecFrom));
          var nEndX = valueFromPercent(barWidth, percentOf(totalRange, nSecTo));
          var range_rect = { x: nStartX, y: 0, w: nEndX - nStartX, h: $bar.height() };
          var getRect = getRectUsing$Position;
          var siblings_rects = [];
          $bar.children('.range').each(function () {
            siblings_rects.push(getRect(this));
          });
          var overlaps = $(range_rect).overlapsX(siblings_rects);
          if( overlaps.length > 0 ) {
            return false;
          }
          var range = { start: nSecFrom, end: nSecTo, teacherID: nTeacher, subjectID: nSubject };
          $bar.rangebar('addFloatRange', range);
          return true;
        },
        applyRange: function($range, strStart, strEnd, nTeacher, nSubject) {
          var $bar = $range.parent();
          var range = $range.data("range");
          var barWidth = $bar.width();
          var options = $bar.data("assil-rangebar").options;
          var totalRange = options.max - options.min;
          var range_rect = this.getRelativeUIRectFromRange(range);
          var nSecFrom = $bar.data("float-options").timeToSec(strStart);
          var nSecTo = $bar.data("float-options").timeToSec(strEnd);
          var nStartX = valueFromPercent(barWidth, percentOf(totalRange, nSecFrom));
          var nEndX = valueFromPercent(barWidth, percentOf(totalRange, nSecTo));
          range_rect.x = nStartX; range_rect.w = nEndX - nStartX;
          var getRect = getRectUsing$Position;
          var siblings_rects = [];
          // find all the sibling ranges...
          $range.siblings('.range').each(function () {
            siblings_rects.push(getRect(this));
          });
          // if overlaped then remove it, not overlaped increase the id...
          var overlaps = $(range_rect).overlapsX(siblings_rects);
          if( overlaps.length > 0 ) {
            return false;
          }
          // not overlaped, update range...
          $range.children(".ui-resizable-w").find(".ui-slider-tip").html( strStart );
          $range.children(".ui-resizable-e").find(".ui-slider-tip").html( strEnd );
          range.teacherID = nTeacher; range.subjectID = nSubject;
          range.start = nSecFrom; range.end = nSecTo;
          $range.data("range", range);
          // update ui range...
          this.updateRangeUI($range);
          return true;
        },
        getRanges: function () {
            var ranges = [];
            //syncRange({ target: this.element });
            $(this.element).children('.range').each(function () {
                var $range = $(this);
                var range = $range.data('range');
                if (!range) return true;
                ranges.push(range);
            });
            ranges.sort(function (a, b) {
                if (a.start < b.start) return -1;
                if (a.start > b.start) return 1;
                if (a.end < b.end) return -1
                if (a.end > b.end) return 1
                return 0;
            });
            return ranges;
        },
        addRange: function (range) {
            var options = this.options;
            var totalRange = options.max - options.min;
            range.id = "r_" + g_assil.cur_max_id;
            range = $.fn.extend({}, options.defaultRange, range);
            ++g_assil.cur_max_id;

            var $range = $("<div class='range' tabindex='1'>").data('range', range);
            var $labelHandle = $range.append("<div class='range-label'>&nbsp;</div>");

            this.element.append($range);
            if (range.css) {
                $range.addClass(range.css.range);
                $labelHandle.addClass(range.css.label);
            }
            
            //syncRange({ target: $range });
            this.updateRangeUI($range);
            if (options.renderRange) options.renderRange($range, range);

            $range.on('mousedown', range_mouse_down);
            $range.on('mouseup', range_mouse_up);

            if (range.disabled) {
                $range.addClass("disabled");
                return true;
            }

            $range
                .draggable({
                    containment: this.element,
                    scroll: false,
                    axis: "x",
                    handle: '.range-label',
                    start: range_drag_start,
                    drag: range_drag_drag,
                    stop: range_drag_stop
                })
                .resizable({
                    containment: this.element,
                    handles: "e, w",
                    resize: range_resize_resize,
                    start: range_resize_start,
                    stop: range_resize_stop
                });

            $range.on('keydown', range_keydown)
                  .on('dblclick', range_dblclick);
            return $range;
        },
        addFloatRange: function(range) {
            // create the range first...
            var $range = this.addRange(range);
            if( $.type($range) != "object" || $range.length <= 0 )
              return false;
            // create the float tips...
            var floatOptions = $(this.element).data("float-options");
            if( floatOptions ) {
              $range.children(".ui-resizable-e").append( $("<span class=\"ui-slider-tip\">"+floatOptions.formatLabel(range.end)+"</span>") );
              $range.children(".ui-resizable-w").append( $("<span class=\"ui-slider-tip\">"+floatOptions.formatLabel(range.start)+"</span>") );
            }
            return $range;
        },
        getRangeByID: function(rangeId) {
            var $ranges = this.element.children('.range');
            for(var i = 0; i < $ranges.length; ++i) {
              var $range = $($ranges[i]);
              var range = $range.data("range");
              if( range.id == rangeId )
                return $range;
            }
            return false;
        },
        removeRange: function (rangeId) {
            var $ranges = this.element.children();
            $ranges.each(function () {
                var $range = $(this);
                var range = $range.data("range");

                if (!range) return true;
                if (range.id != rangeId) return true;

                $range.remove();
                return false;
            });
        },
        setRanges: function (ranges) {
            var _bar = this;
            $(_bar.element.children('.range')).remove();
            $.each(ranges, function () {
                _bar.addRange(this);
            });
        },
        updateRangeUI: function ($range) {
            var options = this.options;
            var $bar = $(this.element);
            var range = $range.data("range");
            var range_rect = this.getRelativeUIRectFromRange(range);
            var range_left = range_rect.x + $bar.offset().left;
            var range_top  = range_rect.y + $bar.offset().top;

            // update the float tips value....
            if( $bar.data("float-options") ) {
              var strStart = $bar.data("float-options").formatLabel(range.start);
              var strEnd = $bar.data("float-options").formatLabel(range.end);
              $range.children(".ui-resizable-w").find(".ui-slider-tip").html( strStart );
              $range.children(".ui-resizable-e").find(".ui-slider-tip").html( strEnd );
            }
            
            // 2017.07.16 - by jackey => must plus 1, for left and top...
            $range.offset({ left: range_left + 1, top: range_top + 1 });
            $range.width(range_rect.w);
            $range.height(range_rect.h);
            
            if( options.updateRange ) options.updateRange($range, range);
            if( options.label ) $(".range-label", $range).text(options.label($range, range));
            if( g_assil.debgug ) console.log("UI range rect after change:" + JSON.stringify(getRectUsing$Position($range)));

        },
        getRelativeUIRectFromRange: function (range) {
            var $container = $(this.element);

            var totalRange = this.options.max - this.options.min;
            var point = measureRangeRect(totalRange, $container.width(), range);
            var rect = {
                x: point.left, y: 0,
                w: point.right - point.left, h: $container.height()
            };

            if (g_assil.debgug) console.log("relative rect of : " + JSON.stringify(range));
            if (g_assil.debgug) console.log("point:" + JSON.stringify(point));
            if (g_assil.debgug) console.log("relative rect:" + JSON.stringify(rect));

            return rect;
        }
    });
    
    function preventCollision_onResize(event, ui) {
        var getRect = getRectUsing$Position;//ui.size ? getRectUsing$Offset : getRectUsing$Position;
        var $range = $(event.target);
        var range = $range.data("range");
        var $bar = $(event.target).parent();
        var range_rect = { x: ui.position.left, y: ui.position.top, w: ui.size.width, h: ui.size.height };
        //var range_rect = { x: $range.position().left, y: $range.position().top, w: $range.width(), h: $range.height() };
        //var bar_rect = getRect($bar);
        
        // must be range object...
        if( !range ) return;

        //console.log("input ui.position:" + JSON.stringify(ui.position));
        //console.log("input ui.size:" + JSON.stringify(ui.size));
        //console.log("range rect:" + JSON.stringify(range_rect));
        //if (g_assil.debgug) console.log("input ui.position:" + JSON.stringify(ui.position));
        //if (g_assil.debgug) console.log("input mouseOffset:" + JSON.stringify(range_rect_offset));
        //if (g_assil.debgug) console.log("   range position:" + JSON.stringify(range_rect));

        // 2017.06.19 - by jackey => check only for sibiling range except self...
        var siblings_rects = [];
        $range.siblings('.range').each(function () {
          siblings_rects.push(getRect(this));
        });
        
        var overlaps = $(range_rect).overlapsX(siblings_rects);
        if( overlaps.length > 0 ) {
          var lapObj = overlaps[0];
          if( lapObj.overlap.isOverlapLeft ) {
            // process left overlap => must minus exceed number...
            ui.size.width -= lapObj.obstacle.x + lapObj.obstacle.w - ui.position.left;
            ui.position.left = lapObj.obstacle.x + lapObj.obstacle.w;
          } else if( lapObj.overlap.isOverlapRight ) {
            ui.size.width = lapObj.obstacle.x - ui.position.left;
          }
        } else {
          ui.position.left = (ui.position.left < 0 ? 0 : ui.position.left);
          ui.position.left = ((ui.position.left + range_rect.w > $bar.width()) ? $bar.width() - range_rect.w : ui.position.left);
        }
        $bar.trigger("change", [event, ui, $bar, $range]);
    };

    function preventCollision_onDrag(event, ui) {
        var getRect = getRectUsing$Position;//ui.size ? getRectUsing$Offset : getRectUsing$Position;
        var $range = $(event.target);
        var range = $range.data("range");
        var $bar = $(event.target).parent();
        var range_rect = { x: ui.position.left, y: ui.position.top, w: $range.width(), h: $range.height() };
        //var bar_rect = getRect($bar);

        // must be range object...
        if( !range ) return;

        //if (g_assil.debgug) console.log("input ui.position:" + JSON.stringify(ui.position));
        //if (g_assil.debgug) console.log("input mouseOffset:" + JSON.stringify(range_rect_offset));
        //if (g_assil.debgug) console.log("   range position:" + JSON.stringify(range_rect));

        // 2017.06.19 - by jackey => check only for sibiling range except self...
        var siblings_rects = [];
        $range.siblings('.range').each(function () {
          siblings_rects.push(getRect(this));
        });

        var overlaps = $(range_rect).overlapsX(siblings_rects);
        if( overlaps.length > 0 ) {
          var lapObj = overlaps[0];
          if( lapObj.overlap.isOverlapLeft ) {
            ui.position.left = lapObj.obstacle.x + lapObj.obstacle.w;
          } else if( lapObj.overlap.isOverlapRight ) {
            ui.position.left = lapObj.obstacle.x - range_rect.w;
          }
        } else {
          ui.position.left = (ui.position.left < 0 ? 0 : ui.position.left);
          ui.position.left = ((ui.position.left + range_rect.w > $bar.width()) ? $bar.width() - range_rect.w : ui.position.left);
        }
        $bar.trigger("change", [event, ui, $bar, $range]);
    };
    
    function range_resize_resize(event, ui) {
        preventCollision_onResize(event, ui);
        syncRange(event, ui);
    };
    
    function range_resize_start(event, ui) {
      // display all the float tips...
      $(this).find(".ui-slider-tip").each(function() {
        //$(this).css('visibility', 'visible');
        $(this).show();
      });
    };
    
    function range_resize_stop(event, ui) {
      // hide all the float tips...
      $(this).find(".ui-slider-tip").each(function() {
        //$(this).css('visibility', 'hidden');
        $(this).hide();
      });
    };
    
    function range_drag_start(event, ui) {
      // display all the float tips...
      $(this).find(".ui-slider-tip").each(function() {
        //$(this).css('visibility', 'visible');
        $(this).show();
      });
      // sync range...
      syncRange(event, ui);
      $(event.target).addClass("dragging");
    };
    
    function range_drag_drag( event, ui ){
        preventCollision_onDrag(event, ui);
        syncRange(event, ui);
    };
    
    function range_drag_stop( event, ui ){
      // hide all the float tips...
      $(this).find(".ui-slider-tip").each(function() {
        //$(this).css('visibility', 'hidden');
        $(this).hide();
      });
      // sync range...
      syncRange(event, ui);
      $(event.target).removeClass("dragging");
    };

    function range_keydown(ev) {
    };

    // fill range area...
    function range_dblclick(ev) {
      ev.stopPropagation();
      ev.preventDefault();

      var $range = $(this);
      var range = $range.data("range");
      if( !range ) return;

      var $bar = $range.parent();
      var options = $bar.data("assil-rangebar").options;
      //var ranges_siblings = $range.siblings().data("range");
      if( options.dbClickEvent ) {
        var strStart = $bar.data("float-options").formatLabel(range.start);
        var strEnd = $bar.data("float-options").formatLabel(range.end);
        options.dbClickEvent($range, strStart, strEnd);
      }
    };

    function range_mouse_up(ev) {
      // 2017.07.16 - by jackey => hide tips...
      $(this).find(".ui-slider-tip").each(function() {
        $(this).hide();
      });
    }
    
    function range_mouse_down(ev) {
        ev.stopPropagation();
        ev.preventDefault();

        // 2017.07.16 - by jackey => display tips...
        $(this).find(".ui-slider-tip").each(function() {
          $(this).show();
        });
        
        var $range = $(this);
        var range = $range.data("range");
        if (!range) return;

        var $bar = $range.parent();
        var options = $bar.data("assil-rangebar").options;

        var last_selected_range = $($bar.data("selected_range"));
        if (last_selected_range != null) last_selected_range.removeClass("selected");
        $bar.data("selected_range", $range);
        $range.addClass("selected");
        
        // notify the focus range...
        if( ev.which == 1 ) {
          // find all the range, select only one...
          $(".slider").each(function(){
            if( $bar.attr("id") != this.id ) {
              var last_select = $(this).data("selected_range");
              if( last_select != null ) last_select.removeClass("selected");
            }
          });
          // notify the focus event...
          //options.focusEvent($range, range);
        }
        // process the middle mouse click...
        if (ev.which !== 2 || !range.allowDelete) return;
        if ($range.data('deleteConfirm')) {
            // clear time out event...
            clearTimeout(this.deleteTimeout);
            // notify the delete event...
            if( options.deleteEvent ) {
              options.deleteEvent($range, range);
            }
            // remove this range...
            $range.remove();
        } else {
            $range.addClass('delete-confirm');
            $range.data('deleteConfirm', true);
            this.deleteTimeout = setTimeout(function () {
                $range.removeClass('delete-confirm');
                $range.data('deleteConfirm', false);
            }, options.deleteTimeout);
        }
    };

    function syncRange(event, ui) {
        var $range = $(event.target);
        var range = $range.data("range");
        var $bar = $range.parent();
        var options = $bar.data("assil-rangebar").options;
        var totalRange = options.max - options.min;
        var parentWidth = $bar.width();
        var left = (ui && ui.position ? ui.position.left : $range.position().left);
        var width = (ui && ui.size ? ui.size.width : $range.width());

        range = $.fn.extend(range, {
            start: valueFromPercent(totalRange, percentOf(parentWidth, left)), 
            end: valueFromPercent(totalRange, percentOf(parentWidth, left + width)), 
        });
        
        // update the float tips value....
        var strStart = $bar.data("float-options").formatLabel(range.start);
        var strEnd = $bar.data("float-options").formatLabel(range.end);
        $range.children(".ui-resizable-w").find(".ui-slider-tip").html( strStart );
        $range.children(".ui-resizable-e").find(".ui-slider-tip").html( strEnd );

        //$range.offset({ top: $bar.offset().top });
        $range.height($bar.height());
        $range.data("range", range);

        if( options.label ) $(".range-label", $range).text(options.label($range, range));
        if( options.updateRange ) options.updateRange($range, range);
    }
    function measureRangeRect(totalRange, componentWidth, range){
        return {
            left: valueFromPercent(componentWidth, percentOf(totalRange, range.start)), 
            right: valueFromPercent(componentWidth, percentOf(totalRange, range.end))
        };
    };
    function percentOf(total, value){return (value*100)/total;};
    function valueFromPercent(total, percent){return (total*percent)/100;};
}(jQuery));

function getRectUsing$Offset(obj) {
    if (!obj) return obj;
    if (obj.x != undefined && obj.y != undefined && obj.w != undefined && obj.h != undefined) return obj;
    var p = $(obj).offset();
    return {
        x: p.left,
        y: p.top,
        w: $(obj).width(),
        h: $(obj).height()
    };
};

function getRectUsing$Position(obj) {
    if (!obj) return obj;
    if (obj.x != undefined && obj.y != undefined && obj.w != undefined && obj.h != undefined) return obj;
    var p = $(obj).position();
    return {
        x: p.left,
        y: p.top,
        w: $(obj).width(),
        h: $(obj).height()
    };
};

function isOverlapRect(rect1, rect2) {
    // overlapping indicators, indicate which part of the reference object (Rectangle1) overlap one obstacle.
    var ret = {
        isOverlapRight: (rect1.x + rect1.w >= rect2.x && rect1.x <= rect2.x),
        isOverlapLeft: (rect1.x <= rect2.x + rect2.w && rect1.x >= rect2.x),
        isOverlapBottom: (rect1.y + rect1.h > rect2.y && rect1.y <= rect2.y),
        isOverlapTop: (rect1.y <= rect2.y + rect2.h && rect1.y >= rect2.y)
    }; 
    ret.isOverlaped = (ret.isOverlapLeft || ret.isOverlapRight || ret.isOverlapTop || ret.isOverlapBottom);
    return ret;
    //( 
    //    (rect1.x <= rect2.x + rect2.w && rect1.x + rect1.w >= rect2.x) &&
    //    (rect1.y <= rect2.y + rect2.h && rect1.y + rect1.h >= rect2.y)
    //)
};

function isOverlapXRect(rect1, rect2) {
    // overlapping indicators, indicate which part of the reference object (Rectangle1) overlap one obstacle.
    var ret = {
        // 2017.07.18 - by jackey => must except the equal value...
        isOverlapRight: (rect1.x + rect1.w > rect2.x && rect1.x < rect2.x),
        isOverlapLeft: (rect1.x < rect2.x + rect2.w && rect1.x > rect2.x)
    }; 
    ret.isOverlaped = (ret.isOverlapLeft || ret.isOverlapRight);
    return ret;
};

function isOverlapYRect(rect1, rect2) {
    var ret = {
        isOverlapBottom: (rect1.y + rect1.h >= rect2.y && rect1.y <= rect2.y),
        isOverlapTop: (rect1.y <= rect2.y + rect2.h && rect1.y >= rect2.y)
    }; 
    ret.isOverlaped = (ret.isOverlapTop || ret.isOverlapBottom);
    return ret;
};

(function ($) {

    $.fn.measureRects = function () {
        var rects = [];
        this.each(function () {
            rects.push(getRect(this));
        });
        return rects;
    };
  
    /**
     * checks if selector ui elements overlaps over any other ui elements
     * @param obstacles is an array of DOM or JQuery selector \r
     * @param func_isOverlapRect It is the function that will calculate a rectangle collides with another and returning a {isOverlaped: true / false} if not mSQL value defaults to 'isOverlapRect'
     * @param getRectFunction function to get objects bounds as {x: float, y: float, w: float, h: float} default value is 'getRectUsing$Offset'
     */
    $.fn.overlaps = function (obstacles, func_isOverlapRect, getRectFunction) {
        try {
            var elems = [];
            var getRect = getRectFunction || getRectUsing$Offset;
            var computOverlaps = func_isOverlapRect || isOverlapRect;
            this.each(function () {
                var this_selector = this;
                var rect1 = getRect(this_selector);
                $(obstacles).each(function () {
                    var this_obstacle = this;
                    var rect2 = getRect(this_obstacle);

                    var overlap = computOverlaps(rect1, rect2);
                    if (overlap.isOverlaped) {
                        elems.push({
                            src: this_selector,
                            obstacle: this_obstacle, 
                            overlap: overlap
                        });
                        // 2017.06.19 - by jackey => find only one...
                        return false;
                    }
                });
            });
            return elems;
        } catch (e) {
            console.log(e);
        }
    };
    /**
     * checks if selector ui elements overlaps over any other ui elements using only horizontal coordinates
     * @param obstacles is an array of DOM or JQuery selector \r
     * @param getRectFunction function to get objects bounds as {x: float, y: float, w: float, h: float} default value is 'getRectUsing$Offset'
     */
    $.fn.overlapsX = function (obstacles, getRectFunction) {
        return this.overlaps(obstacles, isOverlapXRect, getRectFunction);
    };

    /**
     * checks if selector ui elements overlaps over any other ui elements using only vertical coordinates
     * @param obstacles is an array of DOM or JQuery selector \r
     * @param getRectFunction function to get objects bounds as {x: float, y: float, w: float, h: float} default value is 'getRectUsing$Offset'
     */
    $.fn.overlapsY = function (obstacles, getRectFunction) {
        return this.overlaps(obj, isOverlapYRect, getRectFunction);
    };
}(jQuery));

(function($) {
  var extensionMethods = {
    // pips
    pips: function( settings ) {
      var slider = this,
          i, j, p,
          collection = "",
          mousedownHandlers,
          min = slider._valueMin(),
          max = slider._valueMax(),
          pips = ( max - min ) / slider.options.step,
          $pips;
      var options = {
        first: "label",
        /* "label", "pip", false */
        last: "label",
        /* "label", "pip", false */
        rest: "pip",
        /* "label", "pip", false */
        labels: false,
        /* [array], { first: "string", rest: [array], last: "string" }, false */
        prefix: "",
        /* "", string */
        suffix: "",
        /* "", string */
        step: ( pips > 100 ) ? Math.floor( pips * 0.05 ) : 1,
        /* number */
        formatLabel: function(value) {
            return this.prefix + value + this.suffix;
        }
      };
      // 保存配置到指定的pips-options当中...
      if ( $.type( settings ) === "object" || $.type( settings ) === "undefined" ) {
        $.extend( options, settings );
        slider.element.data("pips-options", options );
      } else {
        /*if ( settings === "destroy" ) {
            destroy();
        } else if ( settings === "refresh" ) {
            slider.element.slider( "pips", slider.element.data("pips-options") );
        }*/
        return;
      }
      // we don't want the step ever to be a floating point or negative
      // (or 0 actually, so we'll set it to 1 in that case).
      slider.options.pipStep = Math.abs( Math.round( options.step ) ) || 1;
      // get rid of all pips that might already exist.
      slider.element
        .off( ".selectPip" )
        .addClass("ui-slider-pips")
        .find(".ui-slider-pip")
        .remove();
      // do destory...
      function destroy() {
        slider.element
          .off(".selectPip")
          .on("mousedown.slider", slider.element.data("mousedown-original") )
          .removeClass("ui-slider-pips")
          .find(".ui-slider-pip")
          .remove();
      }
      // method for creating a pip. We loop this for creating all
      // the pips.
      function createPip( which ) {
        var label,
            percent,
            number = which,
            classes = "ui-slider-pip",
            css = "",
            labelValue,
            classLabel,
            labelIndex;
        if ( which === "first" ) {
            number = 0;
        } else if ( which === "last" ) {
            number = pips;
        }
        // labelValue is the actual value of the pip based on the min/step
        labelValue = min + ( slider.options.step * number );
        // classLabel replaces any decimals with hyphens
        classLabel = labelValue.toString().replace(".", "-");
        // get the index needed for selecting labels out of the array
        labelIndex = ( number + min ) - min;
        // we need to set the human-readable label to either the
        // corresponding element in the array, or the appropriate
        // item in the object... or an empty string.
        if ( $.type(options.labels) === "array" ) {
          // 2017.06.16 - by jackey => slider.options.pipStep
          label = options.labels[ labelIndex / slider.options.pipStep ] || "";
        } else if ( $.type( options.labels ) === "object" ) {
          if ( which === "first" ) {
            // set first label
            label = options.labels.first || "";
          } else if ( which === "last" ) {
            // set last label
            label = options.labels.last || "";
          } else if ( $.type( options.labels.rest ) === "array" ) {
            // set other labels, but our index should start at -1
            // because of the first pip.
            label = options.labels.rest[ labelIndex - 1 ] || "";
          } else {
            // urrggh, the options must be f**ked, just show nothing.
            label = labelValue;
          }
        } else {
          label = labelValue;
        }
        if ( which === "first" ) {
          // first Pip on the Slider
          percent = "0%";
          classes += " ui-slider-pip-first";
          classes += ( options.first === "label" ) ? " ui-slider-pip-label" : "";
          classes += ( options.first === false ) ? " ui-slider-pip-hide" : "";
        } else if ( which === "last" ) {
          // last Pip on the Slider
          percent = "100%";
          classes += " ui-slider-pip-last";
          classes += ( options.last === "label" ) ? " ui-slider-pip-label" : "";
          classes += ( options.last === false ) ? " ui-slider-pip-hide" : "";
        } else {
          // all other Pips
          percent = (( 100 / pips ) * which ).toFixed(4) + "%";
          classes += ( options.rest === "label" ) ? " ui-slider-pip-label" : "";
          classes += ( options.rest === false ) ? " ui-slider-pip-hide" : "";
        }
        classes += " ui-slider-pip-" + classLabel;
        css = ( slider.options.orientation === "horizontal" ) ? ("left: " + percent) : ("bottom: " + percent);
        // add this current pip to the collection...
        return "<span class=\"" + classes + "\" style=\"" + css + "\">" +
                "<span class=\"ui-slider-line\"></span>" +
                "<span class=\"ui-slider-label\" data-value=\"" +
                labelValue + "\">" + options.formatLabel(label) + "</span>" + "</span>";
      }
      // create our first pip
      collection += createPip("first");
      // for every stop in the slider where we need a pip; create one.
      for ( p = slider.options.pipStep; p < pips; p += slider.options.pipStep ) {
          collection += createPip( p );
      }
      // create our last pip
      collection += createPip("last");
      // append the collection of pips.
      slider.element.append( collection );
      // store the pips for setting classes later.
      $pips = slider.element.find(".ui-slider-pip");
      
      // store the mousedown handlers for later, just in case we reset
      // the slider, the handler would be lost!
      /*var theEvent = $._data( slider.element.get(0), "events");
      if ( theEvent.mousedown && theEvent.mousedown.length ) {
        mousedownHandlers = theEvent.mousedown;
      } else {
        mousedownHandlers = slider.element.data("mousedown-handlers");
      }
      slider.element.data("mousedown-handlers", mousedownHandlers.slice() );
      // loop through all the mousedown handlers on the slider,
      // and store the original namespaced (.slider) event handler so
      // we can trigger it later.
      for ( j = 0; j < mousedownHandlers.length; j++ ) {
        if ( mousedownHandlers[j].namespace === "rangebar" ) {
          slider.element.data("mousedown-original", mousedownHandlers[j].handler );
        }
      }*/
      
      // set closest range item...
      function setClosestRange( val ) {
        if( $.type(val) != "number" ) {
          return false;
        }
        var $ranges = slider.element.children('.range');
        var minDist = -1, minItem = 0, minRange = false;
        for(var i = 0; i < $ranges.length; ++i) {
          var $range = $($ranges[i]);
          var range = $range.data("range");
          if( minDist < 0 ) { 
            minItem = range.start;
            minDist = Math.abs(minItem - val);
            minRange = $range;
          }
          if( Math.abs(range.start - val) <= minDist ) {
            minItem = range.start;
            minDist = Math.abs(minItem - val);
            minRange = $range;
          }
          if( Math.abs(range.end - val) <= minDist ) {
            minItem = range.end;
            minDist = Math.abs(minItem - val);
            minRange = $range;
          }
        }
        if( minDist < 0 || !minRange )
          return false;
        // check the distance...
        range = minRange.data("range");
        if( minItem == range.start ) {
          //console.log("min start: "+minItem+" Dist: "+minDist+" val: "+val);
          range.start = val;
          minRange.data("range", range);
          slider.updateRangeUI(minRange);
          return true;
        }
        if( minItem == range.end ) {
          //console.log("min end: "+minItem+" Dist: "+minDist+" val: "+val);
          range.end = val;
          minRange.data("range", range);
          slider.updateRangeUI(minRange);
          return true;
        }
        return false;
      }
      // unbind the mousedown.slider event, because it interferes with
      // the labelClick() method (stops smooth animation), and decide
      // if we want to trigger the original event based on which element
      // was clicked.
      slider.element
        .off("mousedown.slider")
        .on("mousedown.selectPip", function(e) {
          var $target = $(e.target);
          var value = $target.data("value");
          if( $target && $.type(value) == "number" ) {
            setClosestRange(value);
          }
        });
      
      /*slider.element.on( "slide.selectPip slidechange.selectPip", function(e, ui) {
          var $slider = $(this),
              value = $slider.slider("value"),
              values = $slider.slider("values");
          if ( ui ) {
              value = ui.value;
              values = ui.values;
          }
          if ( slider.values() && slider.values().length ) {
              selectPip.range( values );
          } else {
              selectPip.single( value );
          }
      });*/
    },
    // floats
    float: function( settings ) {
      var i, slider = this,
        min = slider._valueMin(),
        max = slider._valueMax();
        //$handles = slider.element.find(".ui-resizable-handle");
      var options = {
        time: false,
        handle: true,
        formatLabel: function(value) {
          // process time format display...
          if( this.time ) {
            var theSecond = parseInt(value % 60);
            var theMinute = parseInt(value / 60) % 60;
            var theHour = parseInt(value / 3600);
            theSecond = (theSecond >= 0 && theSecond < 10) ? ((theSecond==0)?"00":"0"+theSecond) : theSecond;
            theMinute = (theMinute >= 0 && theMinute < 10) ? ((theMinute==0)?"00":"0"+theMinute) : theMinute;
            theHour   = (theHour >= 0 && theHour < 10) ? ((theHour==0)?"00":"0"+theHour) : theHour;
            value     = theHour+':'+theMinute+':'+theSecond;
          }
          return value;
        },
        timeToSec: function(strTime) {
          var arrTime = strTime.split(':');
          var nSecond = parseInt(arrTime[0])*3600 + parseInt(arrTime[1])*60 + parseInt(arrTime[2]);
          return nSecond;
        }
      };
      // store the new setting...
      if ( $.type( settings ) === "object" || $.type( settings ) === "undefined" ) {
        $.extend( options, settings );
        slider.element.data("float-options", options );
      }
      // add a class for the CSS
      slider.element
          .addClass("ui-slider-float")
          .find(".ui-slider-tip")
          .remove();
      // append the float tips => for every resize node...
      slider.element.children('.range').each(function() {
        var nEnd = $(this).data("range").end;
        var nStart = $(this).data("range").start;
        $(this).children(".ui-resizable-e").append( $("<span class=\"ui-slider-tip\">"+options.formatLabel(nEnd)+"</span>") );
        $(this).children(".ui-resizable-w").append( $("<span class=\"ui-slider-tip\">"+options.formatLabel(nStart)+"</span>") );
      });
    }
  };
  $.extend(true, $.assil.rangebar.prototype, extensionMethods);
}(jQuery));

/**
* Detect Element Resize Plugin for jQuery
*
* https://github.com/sdecima/javascript-detect-element-resize
* Sebastian Decima
*
* version: 0.5.3
**/

(function ( $ ) {
	var attachEvent = document.attachEvent,
		stylesCreated = false;
	
	var jQuery_resize = $.fn.resize;
	
	$.fn.resize = function(callback) {
		return this.each(function() {
			if(this == window)
				jQuery_resize.call(jQuery(this), callback);
			else
				addResizeListener(this, callback);
		});
	}

	$.fn.removeResize = function(callback) {
		return this.each(function() {
			removeResizeListener(this, callback);
		});
	}
	
	if (!attachEvent) {
		var requestFrame = (function(){
			var raf = window.requestAnimationFrame || window.mozRequestAnimationFrame || window.webkitRequestAnimationFrame ||
								function(fn){ return window.setTimeout(fn, 20); };
			return function(fn){ return raf(fn); };
		})();
		
		var cancelFrame = (function(){
			var cancel = window.cancelAnimationFrame || window.mozCancelAnimationFrame || window.webkitCancelAnimationFrame ||
								   window.clearTimeout;
		  return function(id){ return cancel(id); };
		})();

		function resetTriggers(element){
			var triggers = element.__resizeTriggers__,
				expand = triggers.firstElementChild,
				contract = triggers.lastElementChild,
				expandChild = expand.firstElementChild;
			contract.scrollLeft = contract.scrollWidth;
			contract.scrollTop = contract.scrollHeight;
			expandChild.style.width = expand.offsetWidth + 1 + 'px';
			expandChild.style.height = expand.offsetHeight + 1 + 'px';
			expand.scrollLeft = expand.scrollWidth;
			expand.scrollTop = expand.scrollHeight;
		};

		function checkTriggers(element){
			return element.offsetWidth != element.__resizeLast__.width ||
						 element.offsetHeight != element.__resizeLast__.height;
		}
		
		function scrollListener(e){
			var element = this;
			resetTriggers(this);
			if (this.__resizeRAF__) cancelFrame(this.__resizeRAF__);
			this.__resizeRAF__ = requestFrame(function(){
				if (checkTriggers(element)) {
					element.__resizeLast__.width = element.offsetWidth;
					element.__resizeLast__.height = element.offsetHeight;
					element.__resizeListeners__.forEach(function(fn){
						fn.call(element, e);
					});
				}
			});
		};
		
		/* Detect CSS Animations support to detect element display/re-attach */
		var animation = false,
			animationstring = 'animation',
			keyframeprefix = '',
			animationstartevent = 'animationstart',
			domPrefixes = 'Webkit Moz O ms'.split(' '),
			startEvents = 'webkitAnimationStart animationstart oAnimationStart MSAnimationStart'.split(' '),
			pfx  = '';
		{
			var elm = document.createElement('fakeelement');
			if( elm.style.animationName !== undefined ) { animation = true; }    
			
			if( animation === false ) {
				for( var i = 0; i < domPrefixes.length; i++ ) {
					if( elm.style[ domPrefixes[i] + 'AnimationName' ] !== undefined ) {
						pfx = domPrefixes[ i ];
						animationstring = pfx + 'Animation';
						keyframeprefix = '-' + pfx.toLowerCase() + '-';
						animationstartevent = startEvents[ i ];
						animation = true;
						break;
					}
				}
			}
		}
		
		var animationName = 'resizeanim';
		var animationKeyframes = '@' + keyframeprefix + 'keyframes ' + animationName + ' { from { opacity: 0; } to { opacity: 0; } } ';
		var animationStyle = keyframeprefix + 'animation: 1ms ' + animationName + '; ';
	}
	
	function createStyles() {
		if (!stylesCreated) {
			//opacity:0 works around a chrome bug https://code.google.com/p/chromium/issues/detail?id=286360
			var css = (animationKeyframes ? animationKeyframes : '') +
					'.resize-triggers { ' + (animationStyle ? animationStyle : '') + 'visibility: hidden; opacity: 0; } ' +
					'.resize-triggers, .resize-triggers > div, .contract-trigger:before { content: \" \"; display: block; position: absolute; top: 0; left: 0; height: 100%; width: 100%; overflow: hidden; } .resize-triggers > div { background: #eee; overflow: auto; } .contract-trigger:before { width: 200%; height: 200%; }',
				head = document.head || document.getElementsByTagName('head')[0],
				style = document.createElement('style');
			
			style.type = 'text/css';
			if (style.styleSheet) {
				style.styleSheet.cssText = css;
			} else {
				style.appendChild(document.createTextNode(css));
			}

			head.appendChild(style);
			stylesCreated = true;
		}
	}
	
  window.addResizeListener = function(element, fn){
    if (attachEvent) {
      element.attachEvent('onresize', fn);
    } else {
      if (!element.__resizeTriggers__) {
        if (getComputedStyle(element).position == 'static') {
          element.style.position = 'relative';
        }
        createStyles();
        element.__resizeLast__ = {};
        element.__resizeListeners__ = [];
        (element.__resizeTriggers__ = document.createElement('div')).className = 'resize-triggers';
        element.__resizeTriggers__.innerHTML = '<div class="expand-trigger"><div></div></div>' +
                                            '<div class="contract-trigger"></div>';
        element.appendChild(element.__resizeTriggers__);
        resetTriggers(element);
        element.addEventListener('scroll', scrollListener, true);
        
        /* Listen for a css animation to detect element display/re-attach */
        animationstartevent && element.__resizeTriggers__.addEventListener(animationstartevent, function(e) {
          if(e.animationName == animationName)
            resetTriggers(element);
        });
      }
      element.__resizeListeners__.push(fn);
    }
  };
	
	window.removeResizeListener = function(element, fn){
		if (attachEvent) element.detachEvent('onresize', fn);
		else {
			element.__resizeListeners__.splice(element.__resizeListeners__.indexOf(fn), 1);
			if (!element.__resizeListeners__.length) {
					element.removeEventListener('scroll', scrollListener);
					element.__resizeTriggers__ = !element.removeChild(element.__resizeTriggers__);
			}
		}
	}
}( jQuery ));