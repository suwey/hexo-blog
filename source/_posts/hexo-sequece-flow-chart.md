---
title: 在hexo中写时序图和UML流程图
date: 2016-03-27 10:57:15
tags:  [js-sequece-diagrams, flowchart.js]
categories: hexo  
---

偶然的机会发现CSDN博客有markdown编辑器，点开一看发现支持用markdown写时序图和流程图感到非常给力，于是想将其用在hexo博客中，搜了一下发现很遗憾没有类似的插件就只能自己动手丰衣足食了。其实在最开始玩hexo的时候基于[jointjs](http://www.jointjs.com)把类图搞了一张到博客玩了下，那时候因为没有对应语法所以每次要把代码在html里面写好然后copy到博客文章里，显然玩了几次后面谁也不想玩了，这次CSDN编辑器里面就提供了比较简单的语法然后根据语法生成图形这样易用性就非常好了。打开编辑器直接拖到页面下方就可以看到编辑器使用的开源项目，貌似整个编辑器都是学的[stackedit](https://stackedit.io/editor)，关于两种图的项目分别是[js-sequence-diagrams](https://github.com/bramp/js-sequence-diagrams)和[flowchart.js](https://github.com/adrai/flowchart.js)，那么就看能不能在hexo中用上了。

<!-- more -->

## 尝试写成插件 ##
最开始是想学着官方写个插件，既然官方可以解决highlight语法高亮的问题那么可以仿照解决这个问题，只需要解析对应的语法为某个div然后引用上面两个项目初始化渲染就可以了。于是就稍微研究了下hexo的原理，找到了[hexo-renderer-marked](https://github.com/hexojs/hexo-renderer-marked)是解决highlight的地方，刚小小的兴奋了一下发现里面是调用[marked](https://github.com/chjj/marked)实现的，这就不好搞了，以我的功底直接去改这种项目还是很虚的，更别说在尝试过程中还遇到一些头疼的问题：

1. npm经常卡住，这个好解决直接换成cnpm就好多了
2. js-sequence-diagrams这个项目没发布到npm，于是我自己fork了一个
3. 既然自己fork的那就直接用git安装吧，又发现虽然`npm install -h`明明说支持`git://`但是我死活也装不上，只能改成`git+https://git@github.com...`才行
4. 终于可以下载了结果发现js-sequence-diagrams项目的package.json没有写version无法安装，于是自己加上version
5. 以为天下太平了结果发现flowchart.js项目依赖的[rapheal](http://raphaeljs.com/)写法是`git://`，改之，结果发现后者依赖一个叫`eve`的项目写的也是`git://`，果断fork了后者再改之
6. 本来以为到此已经可以见到太阳了，但是当我照着搜索来的nodejs module写法写完第一句require然后运行的时候看到了...

```
/home/suwey/hexo-renderer-chart/node_modules/.npminstall/raphael/2.1.4/raphael/dev/raphael.core.js:96
            doc: document,
                 ^

ReferenceError: document is not defined
    at /home/suwey/hexo-renderer-chart/node_modules/.npminstall/raphael/2.1.4/raphael/dev/raphael.core.js:96:18
    at /home/suwey/hexo-renderer-chart/node_modules/.npminstall/raphael/2.1.4/raphael/dev/raphael.core.js:15:26
    at Object.<anonymous> (/home/suwey/hexo-renderer-chart/node_modules/.npminstall/raphael/2.1.4/raphael/dev/raphael.core.js:19:2)
```

然后我只能放弃了，继续研究可能对于nodejs大牛们来说不是事但是对于边搜索边做的我来说已经耗费了太大的精力了，毕竟对于我来说只是想好好写博客而已。那么就这样放弃这么好的功能吗？可能愿意用hexo写博客的99%都是技术相关的，流程图这类可能还真的需要用，如果先在软件里面画然后截图效果有时候不是很好，放上来会很模糊，而且相比之下IDE就可以自动生成的类图可能还真没有时序图和流程图需要画的多，那么再换个思路试试吧。

## 不转换只拼接  ##
前面插件写法的思路是想直接把源文件的内容转换成图形代码生成到最终的html里面去，既然这样不可行（至少对于我来说），那么可不可以只是在生成文件后将对应语法的段落转换为带有特定标识的`<div>`然后在文件末尾添加上初始化的代码就好了呢？先复制一段时序图的示例看看会生成怎样的结果：


![seq-syntax](/attachments/img/seq-syntax.jpg)


观察到默认是会生成为`highlight plain`的格式（这里我放了一个截图的原因是如果把这段直接写出来现在就会生成为时序图，如果外层再包一个`highlight plain`的语法的话hexo解析嵌套的语法会导致显示错误），那么证明和highlight处理有关，查阅代码发现是引用了[hexo-util](https://github.com/hexojs/hexo-util)的代码，阅读代码发现是在`function highlight(str, options)`中进行了判断，其会自动推测highlight的语言类型如果无法判断则默认为plain格式，显然这里的sequence是无法识别的，那么只需要修改hexo-util中的hightlight.js文件即可解决将对应语法的段落转化为特定标识的`<div>`的问题。

## 修改hexo-util ##
因为语法和hexo中highlight的语法完全一样，所以修改hexo-util项目lib目录下的highlight.js文件，修改后完整文件在[hexo-util/lib/highlight.js](/attachments/hexo-util/highlight.js)，首先修改`function highlight(str, options)`如下：

```js
var result = {
    value: encodePlainString(str),
    language: lang.toLowerCase()
};

result.isChart = false;
if (result.language === 'sequence' || result.language === 'flow') {
    result.isChart = true;
}

if (result.language === 'plain' || result.isChart) {
  return result;
}
```

这样在遇到`sequence`或者`flow`的时候就不会解析为`plain`格式了，这里给结果增加了一个`isChart`属性，且如果为真直接返回不再进行语言判断，其返回后在`function highlightUtil(str, options)`中处理，需要修改此函数使其不再按照highlight格式生成html，具体如下：

```js
 var caption = options.caption;
 var tab = options.tab;
 var data = highlight(str, options);
 if (data.isChart) {
     return charResult(data);
 }
 if (!wrap) return data.value;
 
 var lines = data.value.split('\n');
```

这里发现是图表数据则直接交给`charResult`函数处理，此函数实现很简单：

```js
function charResult(data) {
  var result = '<div class="' + data.language  + '">';
  result += data.value;
  result += '</div>';
  return result;
}
```

至此就完成了将图表语法处理成带有class标签的div工作，那么后面就是将html中的div变成图表了。

## 修改主题完成图表渲染 ##

这一步就比较简单了，通常是找到主题的模板文件进行修改，一般修改页面底部即可，这里我是在本主题的`/layout/_partial/after-footer.ejs`中添加了这样一段：

```ejs
<% if (theme.chart){ %>
<%- js('js/raphael') %>
<%- js('js/underscore') %>
<%- js('js/sequence-diagram') %>
<%- js('js/flowchart') %>
<%- partial('chart') %>
<% } %>
```

判断主题是否需要开启图表功能，这里需要在主题的`_config.yml`里加一个开关：

```
# Support sequence & flow chart
chart: true
```

别忘了在主题`source/js`目录下添加`raphael`等js文件，然后在`/layout/_partial`目录下新建一个`chart.ejs`文件，里面放上处理图表的代码：

```ejs
<% if (theme.chart){ %>
<!-- Chart Render -->
<script type="text/javascript">
  $(".sequence").sequenceDiagram({theme: 'simple'});
  var flowCount = 0;
  $(".flow").each(function() {
      var el = $(this);
      el.hide();
      el.after('<div id="flow-' + flowCount + '"></div>');
      var chart = flowchart.parse(el.text());
      chart.drawSVG('flow-' + flowCount);
      flowCount++;
  });
</script>
<!-- End Chart Render -->
<% } %>
```

这里对`sequence`的处理比较简单，因为支持直接在语句元素上画图所以只需要一句初始化语句就可以了，但是`flow`这种因为默认不支持直接在元素上画图表，而且还需要另外一个单独的`<div>`作为图表的容器，如果不指定或者试图使用写着语句的div上画图则会导致图表位置不可控，因此需要自己遍历所有图表依次隐藏语句div并添加一个唯一id的容器div然后初始化。

## 总结 ##
至此，通过对hexo-util和主题文件的修改联合完成了将图表语法生成为html和将html渲染成图表的两个步骤，可以在hexo中愉快的使用时序图和UML流程图了。在测试过程中发现本主题引用的`require.js`和`raphael.js`等图表使用的js会产生冲突，提示为`Mismatch anonymous define module`，本来试图在搜索的指引下解决此问题结果发现将`raphael`等几个js文件改得一塌糊涂后依然是云里雾里，为了免得图表组件更新后又稀里糊涂的乱改一气最后只得将本主题几个使用了require的js文件全部修改为了原生的写法。所以本次虽然最终达到了目的但是无论是对hexo的修改还是对图表js等的引用可能都不是最优方案，这里就当作是抛砖引玉了希望以后可以看到更好的解决方式，最后既然说了这么多当然得放上几个图检验下工作成果，那就都放在末尾了。

### 时序图
```sequence
Alice->Bob: Hello Bob, how are you?
Note right of Bob: Bob thinks
Bob-->Alice: 我很好 thanks!
```

### 流程图1
```flow
st=>start: Start|past:>http://www.google.com[blank]
e=>end: End:>http://www.google.com
op1=>operation: My Operation|past
op2=>operation: Stuff|current
sub1=>subroutine: My Subroutine|invalid
cond=>condition: Yes
or No?|approved:>http://www.google.com
c2=>condition: Good idea|rejected
io=>inputoutput: catch something中文|request

st->op1(right)->cond
cond(yes, right)->c2
cond(no)->sub1(left)->op1
c2(yes)->io->e
c2(no)->op2->e
```

### 流程图2
```flow
st=>start: Start
e=>end
op=>operation: My Operation
cond=>condition: Yes or No?

st->op->cond
cond(yes)->e
cond(no)->op
```
