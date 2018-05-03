<?php
/**
 * Markdown 处理函数
 *
 * $Id: markdown.php  i@annhe.net  2015-6-18 $
 **/

if(!defined('IN_DISCUZ')) {
	exit('Access Denied');
}

require_once(dirname(__FILE__) . '/' . '../tools/parsedown/ExtParsedown.php');

class Markdown {
	function header() {
		global $_G;
		@extract($_G['cache']['plugin']['zxsq_markdown']);

		$dir = "source/plugin/zxsq_markdown/";
		$css = $dir . "css/markdown.css";
		$tocjs = $dir . "js/tocbot.min.js";
		$toccss = $dir . "css/tocbot.min.css";
		$markdownStyle = (trim($markdownStyle) == "" ? $css : trim($markdownStyle));
		$tocJs = (trim($tocJs) == "" ? $tocjs : trim($tocJs));
		$tocCss = (trim($tocCss) == "" ? $toccss : trim($tocCss));
		if($toc)
		{
			$tocLink = '<script src="' . $tocJs . '"></script>'.'<link rel="stylesheet" href="' . $tocCss . '">';
		}else
		{
			$tocLink = "";
		}

		$parsecss = $tocLink . "<link rel=\"stylesheet\" href=\"" . $markdownStyle . "\" />";
		return $parsecss;
	}

	function run($texcode, $pid = "") {
		//echo "Raw\n" . $texcode . "\n\n";
		$regex = array();
		$regex['gt'] = '/^(\t)?(\s{4,7})?&gt;([&gt;]*\s?)/m';

		$texcode = preg_replace_callback($regex['gt'], array($this,'callback_blockquote'), $texcode);
		$texcode = str_replace("&quot;", "\"", $texcode);
		$texcode = str_replace("&amp;", "&", $texcode);
		$texcode = preg_replace("|\[url\](.+?)\[/url\]|m", "\\1", $texcode);
		$texcode = preg_replace("|\[url=(.+?)\](.+?)\[/url\]|m", "\\1", $texcode);
		$texcode = preg_replace("|\[img\](.+?)\[/img\]|m", "\\1", $texcode);
		$texcode = preg_replace("|\[flash\](.+?)\[/flash\]|m", "\\1", $texcode);
		$texcode = preg_replace("|\[audio\](.+?)\[/audio\]|m", "\\1", $texcode);
		$texcode = preg_replace("|\[email\](.+?)\[/email\]|m", "\\1", $texcode);
		$texcode = preg_replace("#&lt;(\w+[://|@].+?)&gt;#m", "<\\1>", $texcode);

		$parse = new ExtParsedown();
		$parse->setHeadingIdPre($pid);
		$parse->setSafeMode(true);
		$parse->setBreaksEnabled(true);

		$encode = mb_detect_encoding($texcode, array('UTF-8','GBK','EUC-CN','GB2312'));
		//echo "Replace\n" . $texcode . "\n\n";
		if($encode != "UTF-8") {
				$texcode = mb_convert_encoding($texcode, 'UTF-8', $encode);
				$texcode = $parse->text($texcode);
				$texcode = mb_convert_encoding($texcode, $encode, 'UTF-8');
		} else {
				$texcode = $parse->text($texcode);
		}
		
		// 处理bbcode冲突
		$texcode = preg_replace('/\[((?!\/?attach).+?)\]/', "[zxsq-anti-bbcode-\${1}]", $texcode);

		include template('zxsq_markdown:markdown');
		return trim($markdown);

	}

	//处理嵌套引用
	function callback_blockquote($match) {
		$match[3] = str_replace("&gt;", ">", $match[3]);
		return $match[1] . $match[2] . ">" . $match[3];
	}

	//目录ID处理回调函数
	function callback_tocid($match) {
		$id = md5($match[2]);
		return $match[1] . " id=\"$id\">" . $match[2] . $match[3];
	}

}
