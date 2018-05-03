<?php
/**
 *	[编辑器(zxsq_editor.{modulename})] (C)2013-2099 Powered by @tecbbs.com.
 *	Version: v1.0
 *	Date: 2017-6-5
 **/

if(!defined('IN_DISCUZ')) {
	exit('Access Denied');
}

class plugin_zxsq_editor {
	// 获取zxsq相关插件变量值
	function getPluginVar()
	{
		$default = array(
			// flowchart
			'raphaelJs' => "//cdn.bootcss.com/raphael/2.2.7/raphael.min.js",
			'flowchartJs' => "//cdn.bootcss.com/flowchart/1.6.6/flowchart.min.js",
			// ditaa
			'ditaaApi' => "//api.annhe.net/gv/api.php",
			// sequencediagram
			'webfontloaderJs' =>"//cdn.bootcss.com/webfont/1.6.27/webfontloader.js",
			'snapSvgJs' =>"//cdn.bootcss.com/raphael/2.2.7/raphael.min.js",
			'underscoreJs' => "//cdn.bootcss.com/underscore.js/1.8.3/underscore-min.js",
			'sequenceDiagramJs' => "https://bramp.github.io/js-sequence-diagrams/js/sequence-diagram-min.js",
			// mathjax
			'mathjaxServer' => "//cdn.bootcss.com/mathjax/2.7.0/MathJax.js?config=TeX-AMS-MML_HTMLorMML&noContrib",
			// graphviz
			'gvApiUrl' => "//api.annhe.net/gv/api.php",
			// 思维导图
			'api' => "//api.annhe.net/gv/api.php",
			// code
			'hilightStyle' => 'far',
			
			// markdown
			'markdownStyle' => "source/plugin/zxsq_markdown/css/markdown.css",
			
			//mermaid
			'mermaidCss' => "//cdn.bootcss.com/mermaid/7.0.0/mermaid.min.css",
			'mermaidJs' => "//cdn.bootcss.com/mermaid/7.0.0/mermaid.min.js",
		);
		
		global $_G;
		$zxsqVar = array();
		foreach($_G['cache']['plugin'] as $k => $v)
		{
			if(strpos($k, "zxsq_") === 0 && $k != "zxsq_editor")
			{
				$zxsqVar[$k] = array();
				foreach($v as $key => $value)
				{
					if(array_key_exists($key, $default))
					{
						$zxsqVar[$k][$key] = (trim($value) == "" ? $default[$key] : trim($value));
					}else
					{
						$zxsqVar[$k][$key] = trim($value);
					}	
				}
			}
		}
		// 单引号向模板传参有问题
		if(array_key_exists("zxsq_mathjax", $zxsqVar))
		{
			$zxsqVar["zxsq_mathjax"]["mathconfigUser"] = str_replace("'", '"', $zxsqVar["zxsq_mathjax"]["mathconfigUser"]);
		}
		return($zxsqVar);
	}
	
	// 非utf8 json_encode会失败
	function utf8($arr)
	{
		$ret = array();
		foreach($arr as $k => $val)
		{
			if(is_array($val))
			{
				$ret[$k] = $this->utf8($val);
			}else
			{
				$encode = mb_detect_encoding($val, array("ASCII",'UTF-8',"GB2312","GBK",'BIG5'));
				$ret[$k] = mb_convert_encoding($val, 'utf-8', $encode);
			}
		}
		return($ret);
	}

	function post_editorctrl_left() {
		$lang = lang('plugin/zxsq_editor');
		
		global $_G;
		$maximagesize = $_G['group']['maximagesize']/1024;
		$simplemdeJs = "//cdn.bootcss.com/simplemde/1.11.2/simplemde.min.js";
		$simplemdeCss = "//cdn.bootcss.com/simplemde/1.11.2/simplemde.min.css";
		$FontAwesome = "//cdn.bootcss.com/font-awesome/4.7.0/css/font-awesome.min.css";
		$editorTitle = $lang['main_title'];

		$pluginVar = $_G['cache']['plugin']['zxsq_editor'];
		$pluginVar['editorTitle'] = (trim($pluginVar['editorTitle']) == "" ? $editorTitle : trim($pluginVar['editorTitle']));
		// 单引号向模板传参有问题
		$pluginVar['editorTips'] = str_replace("'", '"', trim($pluginVar['editorTips']));
		$pluginVar['simplemdeJs'] = (trim($pluginVar['simplemdeJs']) == "" ? $simplemdeJs : trim($pluginVar['simplemdeJs']));
		$pluginVar['simplemdeCss'] = (trim($pluginVar['simplemdeCss']) == "" ? $simplemdeCss : trim($pluginVar['simplemdeCss']));
		$pluginVar['FontAwesome'] = (trim($pluginVar['FontAwesome']) == "" ? $FontAwesome : trim($pluginVar['FontAwesome']));
		$pluginVar['menuWidth'] = (trim($pluginVar['menuWidth']) == "" ? 900 : (int)trim($pluginVar['menuWidth']));
		//非utf8 json_encode会失败, js中不需要此变量，故直接定义变量
		$editorLabel = (trim($pluginVar['editorLabel']) == "" ? $lang['label'] : trim($pluginVar['editorLabel']));
		$aPluginVar = $pluginVar;
		$aPluginVar['enabledModule'] = unserialize($aPluginVar['enabledModule']);
		$aPluginVar['maximagesize'] = $maximagesize;
		$pluginVar = json_encode($this->utf8($aPluginVar));
		
		$aZxsqVar = $this->getPluginVar();
		$zxsqVar = json_encode($aZxsqVar); 
		
		$nlang = $this->utf8($lang);
		$params = json_encode($nlang);
		
		include template('zxsq_editor:btn');
		return trim($btn);
	}
}


class plugin_zxsq_editor_forum extends plugin_zxsq_editor {
}

class plugin_zxsq_editor_group extends plugin_zxsq_editor_forum {
}

class mobileplugin_zxsq_editor extends plugin_zxsq_editor {
}

class mobileplugin_zxsq_editor_forum extends plugin_zxsq_editor_forum {
}
