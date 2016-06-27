---
title: Hello World Hexo3
categories: hexo
---
Welcome to [Hexo](http://hexo.io/)! This is your very first post. Check [documentation](http://hexo.io/docs/) for more info. If you get any problems when using Hexo, you can find the answer in [troubleshooting](http://hexo.io/docs/troubleshooting.html) or you can ask me on [GitHub](https://github.com/hexojs/hexo/issues). Deployment settings has been changed and Chinese friends may need VPN to complete `hexo init` or `npm install` or even `hexo deploy`, watch out that `hexo-deployer-git` and `hexo-generator-feed` need to install separately and `hexo-generator-sitemap` may cause error when run `hexo g`.    

<!-- more -->

## Quick Start

### Create a new post

``` bash
$ hexo new "My New Post"
```

More info: [Writing](http://hexo.io/docs/writing.html)

### Run server

``` bash
$ hexo server
```

More info: [Server](http://hexo.io/docs/server.html)

### Generate static files

``` bash
$ hexo generate
```

More info: [Generating](http://hexo.io/docs/generating.html)

### Deploy to remote sites

``` bash
$ hexo deploy
```

More info: [Deployment](http://hexo.io/docs/deployment.html)

### Update 

My way is `npm update -g hexo-cli`, `hexo init newblog`, `cp newblog/package.json oldblog/`, watch out if you have changed `oldblog/package.json`, try `vimdiff oldblog/package.json newblog/package.json` first, and the last hit `cd oldblog; rm -rf node_modules; npm install`. 
