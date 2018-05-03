<?php
if(!defined('IN_DISCUZ')) {
	exit('Access Denied');
}
class plugin_jia_fanhuidb {
	function plugin_jia_fanhuidb() {
		global $_G;
		$this->jia_fanhuidb = $_G['cache']['plugin']['jia_fanhuidb']['jia_fanhuidb'];
		$this->opacity = $_G['cache']['plugin']['jia_fanhuidb']['opacity'];
		$this->yesopacity = $_G['cache']['plugin']['jia_fanhuidb']['yesopacity'];
		$this->jia_fanhuidb_qq = $_G['cache']['plugin']['jia_fanhuidb']['jia_fanhuidb_qq'];
		$this->wangwang = $_G['cache']['plugin']['jia_fanhuidb']['wangwang'];
		$this->sie = $_G['cache']['plugin']['jia_fanhuidb']['sie'];
		$this->jia_fanhuidb_1 = $_G['cache']['plugin']['jia_fanhuidb']['jia_fanhuidb_1'];
		$this->jia_fanhuidb_2 = $_G['cache']['plugin']['jia_fanhuidb']['jia_fanhuidb_2'];
		$this->jia_fanhuidb_lian1 = $_G['cache']['plugin']['jia_fanhuidb']['jia_fanhuidb_lian1'];
		$this->jia_fanhuidb_lian2 = $_G['cache']['plugin']['jia_fanhuidb']['jia_fanhuidb_lian2'];
	
	}
	public function global_footer() {
        global $_G;		
		include template('jia_fanhuidb:jia_fanhuidb');
	    return $return;	
	}

}
?>