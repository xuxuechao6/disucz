<?php if(!defined('IN_DISCUZ')) exit('Access Denied'); ?><?php
$btn = <<<EOF


EOF;
 if($aZxsqVar['zxsq_mermaid'] || ($aZxsqVar['zxsq_mermaid'] && $aZxsqVar['zxsq_markdown'])) { 
$btn .= <<<EOF

<link rel="stylesheet" href="{$aZxsqVar['zxsq_mermaid']['mermaidCss']}">
<script src="{$aZxsqVar['zxsq_mermaid']['mermaidJs']}" type="text/javascript" charset="utf-8"></script>
<script>mermaid.initialize({startOnLoad:true});</script>

EOF;
 } if($aZxsqVar['zxsq_markdown']) { 
$btn .= <<<EOF

<link rel="stylesheet" href="{$aZxsqVar['zxsq_markdown']['markdownStyle']}">

EOF;
 } if($aZxsqVar['zxsq_flowchart'] && $aZxsqVar['zxsq_sequence']) { 
$btn .= <<<EOF

<script src="{$aZxsqVar['zxsq_flowchart']['raphaelJs']}" type="text/javascript"></script>
<script src="{$aZxsqVar['zxsq_flowchart']['flowchartJs']}" type="text/javascript"></script>
<script src="{$aZxsqVar['zxsq_sequence']['webfontloaderJs']}" type="text/javascript"></script>
<script src="{$aZxsqVar['zxsq_sequence']['underscoreJs']}" type="text/javascript"></script>
<script src="{$aZxsqVar['zxsq_sequence']['sequenceDiagramJs']}" type="text/javascript"></script>

EOF;
 } elseif($aZxsqVar['zxsq_flowchart']) { 
$btn .= <<<EOF

<script src="{$aZxsqVar['zxsq_flowchart']['raphaelJs']}" type="text/javascript"></script>
<script src="{$aZxsqVar['zxsq_flowchart']['flowchartJs']}" type="text/javascript"></script>

EOF;
 } elseif($aZxsqVar['zxsq_sequence']) { 
$btn .= <<<EOF

<script src="{$aZxsqVar['zxsq_sequence']['snapSvgJs']}" type="text/javascript"></script>
<script src="{$aZxsqVar['zxsq_sequence']['webfontloaderJs']}" type="text/javascript"></script>
<script src="{$aZxsqVar['zxsq_sequence']['underscoreJs']}" type="text/javascript"></script>
<script src="{$aZxsqVar['zxsq_sequence']['sequenceDiagramJs']}" type="text/javascript"></script>

EOF;
 } else { } if($aZxsqVar['zxsq_code'] || ($aZxsqVar['zxsq_markdown'] && $aZxsqVar['zxsq_code'])) { 
$btn .= <<<EOF

<link rel="stylesheet" href="source/plugin/zxsq_code/tools/highlight/styles/{$aZxsqVar['zxsq_code']['hilightStyle']}.css"/>
<script src="source/plugin/zxsq_code/tools/highlight/highlight.pack.js" type="text/javascript"></script>
<script>hljs.initHighlightingOnLoad();</script>

EOF;
 } 
$btn .= <<<EOF


<link rel="stylesheet" href="{$aPluginVar['simplemdeCss']}">
<script src="{$aPluginVar['simplemdeJs']}" type="text/javascript" charset="utf-8"></script>
<link href="{$aPluginVar['FontAwesome']}" rel="stylesheet">
<!--<script src="source/plugin/zxsq_editor/js/syncscroll.js" type="text/javascript"></script>-->
<!--<script src="//cdn.bootcss.com/jquery/3.2.1/jquery.min.js" type="text/javascript"></script>-->
<!--<script type="text/javascript">var jq = jQuery.noConflict();</script>-->
<script src="source/plugin/zxsq_editor/libs/ace/ace.js" type="text/javascript" type="text/javascript" charset="utf-8"></script>
<link rel="stylesheet" href="source/plugin/zxsq_editor/css/btn.min.css">
<link rel="stylesheet" href="source/plugin/zxsq_editor/iconfont/iconfont.css">
<script src="source/plugin/zxsq_editor/js/btn.min.js" type="text/javascript"></script>

<a id="zxsq_editor_btn" title="Markdown扩展" onClick='zxsq_editor_btn({$params}, {$pluginVar}, {$zxsqVar})' href="javascript:void(0);">{$editorLabel}</a>


EOF;
 if($aZxsqVar['zxsq_mathjax'] || ($aZxsqVar['zxsq_markdown'] && $aZxsqVar['zxsq_mathjax'])) { 
$btn .= <<<EOF

<script src="{$aZxsqVar['zxsq_mathjax']['mathjaxServer']}" type="text/javascript"></script>
<script type="text/x-mathjax-config">
MathJax.Hub.Config({ 
tex2jax: {inlineMath: [['$','$'], ['\\\\(','\\\\)']]},
skipTags: ['script', 'noscript', 'style', 'textarea', 'pre','code','a'],
showProcessingMessages: false,
messageStyle: "none",
showMathMenu: false,
TeX: { equationNumbers: {autoNumber: "AMS"} },
});
</script>

EOF;
 } if($aZxsqVar['zxsq_superplot']) { 
$btn .= <<<EOF

<script src="source/plugin/zxsq_superplot/js/loadsuperplot.min.js" type="text/javascript"></script>

EOF;
 } 
$btn .= <<<EOF


EOF;
?>
