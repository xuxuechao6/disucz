<?php
/**
 *	[Markdown(zxsq_markdown.{modulename})] (C)2015-2099 Powered by @tecbbs.com.
 *	Version: v1.0
 *	Date: 2015-7-18 12:36
 */

if(!defined('IN_DISCUZ')) {
	exit('Access Denied');
}

include 'function/markdown.php';

class plugin_zxsq_markdown {
	private $postId = 1;
	function checkflag() {
		global $_G;
		if(array_key_exists('zxsq_plugin_flags', $_G)) {
			if(in_array('zxsq_markdown', $_G['zxsq_plugin_flags'])) {
				return True;
			}
		}
		return False;
	}
	
	function setflag($item) {
		global $_G;
		if(array_key_exists("zxsq_plugin_flags", $_G)) {
			$zxsq_plugin_flags = $_G['zxsq_plugin_flags'];
		}else {
			$zxsq_plugin_flags = array();
		}
		$zxsq_plugin_flags[] = $item;
		setglobal("zxsq_plugin_flags", $zxsq_plugin_flags);
	}

	function checkPortal() {
		global $_G;
		if($_G['cache']['plugin']['zxsq_markdown']['portal']) {
			return true;
		}
		return false;
	}

	function global_header() {
		// 按需加载资源文件
		if(!$this->checkflag() && CURMODULE != 'view') {
			return "";
		}

		//markdown css
		$markdown = new Markdown();
		$parsecss = $markdown->header();

		//浏览帖子时才插入脚本
		if(CURMODULE == 'viewthread') {
			return $parsecss;
		}

		if(CURMODULE == 'view' && $this->checkPortal()) {
			return $parsecss;
		}
		return "";
	}
	
	function global_footer() {
		if(CURMODULE == 'view' && $this->checkPortal()) {
			return '<script src="source/plugin/zxsq_markdown/js/portal-markdown.js"></script>';
		}
		return "";
	}

	function discuzcode($param) {
		global $_G;
		//print_r($_G['discuzcodemessage']);	
		// 如果内容中没有 tex 的话则不尝试正则匹配
		if (strpos($_G['discuzcodemessage'], '[/md]') === false || strpos($_G['discuzcodemessage'], '[nomd]') > -1) {
			return false;
		}
		$this->setflag('zxsq_markdown');
		
		// 仅在解析discuzcode时执行对 tex 的解析
		$pattern = '/\s?\[md\][\n\r]*(.+?)[\n\r]*\[\/md\]\s?/s';
		if($param['caller'] == 'discuzcode') {
			$this->postId = $param['param'][12];
			$_G['discuzcodemessage'] = preg_replace_callback($pattern, array($this,'optex'), $_G['discuzcodemessage']);
		}
	}
	function optex($match) {
		$texcode = $match[1];
		$md = new Markdown();
		return $md->run($texcode, $this->postId);
	}
	
}


class plugin_zxsq_markdown_forum extends plugin_zxsq_markdown {
	//回调函数，替换<br />
	function delbr($match) {
		$match[2] = str_replace("<br /><br />", "<markdown-newline-br />", $match[2]);
		$match[2] = str_replace("<br />", "", $match[2]);
		$match[2] = str_replace("<markdown-newline-br />", "<br />", $match[2]);
		$match[2] = str_replace("[zxsq-anti-bbcode-", "[", $match[2]);
		
		//$match[2] = str_replace("[zxsq-anti-bbcode-", "[", $match[2]);
		//替换code中多余的&amp;
		if(substr($match[1], 0, 5) == "<code") {
			$match[2] = str_replace("&amp;", "&", $match[2]);
		}
		// 支持mermaid
		if($match[1] == '<pre><code class="mermaid">')
		{
			return '<div class="mermaid">' . str_replace("&amp;", "&", $match[2]) . '</div>';
		}
		return $match[1] . $match[2] . $match[3];
	}	

	function toc($pid) {
		$script = "<script>tocbot.init({" . 
			"tocSelector: '#markdown_toc_" . $pid . "'," . 
			"contentSelector: '#postmessage_" . $pid . "'," .
			"headingSelector: 'h1, h2, h3'," .
			"collapseDepth: 4," .
			//"ignoreSelector: '.attach_nopermission'," .
			//"smoothScroll: false," .
			"});</script>";
		return($script);
	}
	function viewthread_bottom_output() {
		global $postlist;
		global $_G;
		@extract($_G['cache']['plugin']['zxsq_markdown']);

		$pattern = array();
		$pattern[] = '|(parsedown-markdown")(.+?)(parsedown-markdown-end_FLAG_ZXSQ)|s';
		$pattern[] = '|(<pre><code class="mermaid">)(.+?)(</code></pre>)|s'; // 支持mermaid
		foreach($postlist as $pid => $post) {
			for($i=0; $i<count($pattern); $i++) {
				$post['message'] = preg_replace_callback($pattern[$i],array($this,'delbr'),$post['message']);
				// 不显示[nomd]标签
			}
			$post['message'] = str_replace("[nomd]", "", $post['message']);
			if(strpos($post['message'], "parsedown-markdown") > -1 && $toc)
			{
				$post['message'] .= $this->toc($pid);
			}
			$postlist[$pid] = $post;
		}
		return '';
	}

	function viewthread_sidebottom_output() {
		$ret = array();
		global $postlist;
		foreach($postlist as $pid => $post)
		{
			$ret[] = '<nav class="toc toc-side relative z-1 transition--300 absolute" id="markdown_toc_' . $pid . '"></nav>';
		}
		return $ret;
	}

}

class plugin_zxsq_markdown_group extends plugin_zxsq_markdown_forum {
}

class mobileplugin_zxsq_markdown extends plugin_zxsq_markdown {
	function global_header_mobile() {
		return parent::global_header();
	}
}

class mobileplugin_zxsq_markdown_forum extends plugin_zxsq_markdown_forum {
}
