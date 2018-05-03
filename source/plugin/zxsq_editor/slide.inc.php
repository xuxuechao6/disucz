<?php
if(!defined('IN_DISCUZ')) {
	exit('Access Denied');
}
$markdown = "### No Suport for IE\n\nPlease use Chrome\n\n![](source/plugin/zxsq_editor/css/incompatible_ie.png)![](source/plugin/zxsq_editor/css/compatible_firefox.png)![](source/plugin/zxsq_editor/css/compatible_chrome.png)![](source/plugin/zxsq_editor/css/compatible_safari.png)![](source/plugin/zxsq_editor/css/compatible_opera.png)";
$title = "please use chrome";
$theme = "league";
include template('zxsq_slide:slide');
header("Content-type:text/html;charset=utf-8");
die(trim($slide));
