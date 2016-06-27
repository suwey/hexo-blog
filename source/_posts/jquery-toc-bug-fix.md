---
title: Jquery插件Table of Contents修改
tags: [toc]
categories: js
---

装完博客之后发现其他主题的文章有个新功能：可以在正文右边生成文章的目录，也就是TOC（table of contents)。 于是想把这个功能移植过来，但是不太清楚hexo生成文章的过程，也不想和其他主题一样都固定在文章右边，这样在浏览文章后面时无法方便的切换到其他段落，所以就想能否直接用js抓取正文的标题自动生成一个TOC列表，搜索后发现有个jquery的插件[toc](http://projects.jga.me/toc/)正好有这个功能，然后问题就来了。

<!-- more -->

## 无法真正的smoothScroll ##
装上使用后发现一直不对，具体可以参考bootstrap的关于页面[bootstrap](http://www.bootcss.com/about/)。很明显点击左侧的时候无法真的顺滑滚动（可对比本页面效果），都是直接跳到标题处而且因为有个顶端的nav，标题还会被遮挡（这个不同浏览器表现还不太一样，IE有时候只会遮挡一部分）。这个问题排查了很久，一直到发现无论如何设置参数效果都是这样，遂决定看看源码。

## 排查toc.js源码 ##
使用的时候是安装官网demo的设置设定的，所以要排查问题，先从设置开始看看整个执行的流程。

### 排查执行流程 ###
``` js
$('#toc').toc({
    'selectors': 'h1,h2,h3', //elements to use as headings
    'container': 'body', //element to find all selectors in
    'smoothScrolling': true, //enable or disable smooth scrolling on click
    'prefix': 'toc', //prefix for anchor tags and class names
    ...
```

设置的时候smoothScrolling的值为true,然后看初始化代码。

``` js
$.fn.toc = function(options) {
  var self = this;
  var opts = $.extend({}, jQuery.fn.toc.defaults, options);

  var container = $(opts.container);
  var headings = $(opts.selectors, container);
  var headingOffsets = [];
  var activeClassName = opts.activeClass;

  var scrollTo = function(e, callback) {
    if (opts.smoothScrolling && typeof opts.smoothScrolling === 'function') {
      e.preventDefault();
      var elScrollTo = $(e.target).attr('href');

      opts.smoothScrolling(elScrollTo, opts, callback);
    }
    $('li', self).removeClass(activeClassName);
    $(e.target).parent().addClass(activeClassName);
  };
```

这段代码就是$().toc({})后做的事情了，可以看到初始化的设置被当作options传入，然后对toc.defaults的设置进行了覆盖作为opts变量继续传递。重点看scrollTo方法：if语句先判断opts.smoothScrolling为true然后在判断其类型是function然后才真的进行smoothScrolling，所以问题就在这了，即使在脚本语言里应该也没法让一个变量同时是boolean型又是function吧。这个bug导致scrollTo方法根本起不到预想的作用。

### 解决方案 ###
根据上面的分析，只需要把scrollTo方法里面if语句前面的`opt.smoothScrolling`改成`opt.smoothScroll`然后在使用toc方法设置的时候把`'smoothScrolling': true`改成`'smoothScroll': true`就行了。

### 避免nav遮挡 ###
有些网站会和本站一样有个顶端固定的nav，仅仅按照上面的设置会在跳转的时候造成遮挡损失一部分标题内容，看文档应该设置一个参数`scrollToOffset`，但是这个参数应该设置为多少呢，猜想应该是nav的高度，所以很自然的设置为`$('nav').outerHeight()`。貌似问题解决了，但是既然已经分析到这了顺便看看这个参数如何起效的吧。

## 分析smoothScroll ##
接着上文的scrollTo方法进入可以看到使用了opts.smoothScrolling方法，根据前面的分析opts的值来自初始化的选项覆盖默认选项后的结果，所以看看默认的方法。

``` js
smoothScrolling: function(target, options, callback) {
    $(target).smoothScroller({
      offset: options.scrollToOffset
    }).on('smoothScrollerComplete', function() {
      callback();
    });
  },
```
其中target就是生成的toc中a标签地址href的值，options就是opts，callback的真相得看看下面这段。

``` js
/build TOC item
      var a = $('<a/>')
        .text(opts.headerText(i, heading, $h))
        .attr('href', '#' + anchorName)
        .bind('click', function(e) {
          $(window).unbind('scroll', highlightOnScroll);
          scrollTo(e, function() {
            $(window).bind('scroll', highlightOnScroll);
          });
          el.trigger('selected', $(this).attr('href'));
        });
```
### toc原理 ###

插件的原理是将container容器里面的selector选择器选中的元素（也就是标题）前面生成一个span标签，id为前缀加序号（例如toc0,toc1）。然后生成一个ul列表，列表项为href指向span标签id的地址（例如#toc0,#toc1）。这样其实如果不使用smoothScroll可以直接点击跳转了，但是对本博客这种页面顶端有nav的就会导致标题被遮挡的情况。（所以因为有上文分析的bug存在，是一直都是这样的浏览器默认行为的）

根据这段代码可以得知scrollTo传给smoothScrolling的callback函数就是`highlightOnScroll`。因为scrollTo的判断bug存在，修改之前highlightOnScroll的效果在点击跳转后会失效的，因为在unbind之后没有机会再bind回来了，所以我一度直接把unbind的函数删了。

### smoothScroller ###

回到smoothScrolling方法可以看到：讲生成的ul列表链接的href指向元素上生成了一个smoothScroller对象，传入了一个参数就是初始化设置的`scrollToOffset`。所以看看smoothScroller的代码：

``` js
$.fn.smoothScroller = function(options) {
    options = $.extend({}, $.fn.smoothScroller.defaults, options);
    var el = $(this);

    $(options.scrollEl).animate({
      scrollTop: el.offset().top - $(options.scrollEl).offset().top - options.offset
    }, options.speed, options.ease, function() {
...
```

拿到options后也是对默认值进行了一次覆盖，可以看到默认的`options.scrollEl`的值为`'body,html'`，现在可以清楚看到smoothScroll的执行过程了：在ul列表链接href指向的元素上向上移动一段距离，距离的大小为该元素的到top的距离减去body到top的距离再减去自定义的offset值。

综上所述，smoothScroll就是把生成在选中标题前面span标签向上移动该标签到页面顶端的距离减去offset的距离，也就是初始化设置中的scrollToOffset的值。所以前面设置为nav的高度就对了。
