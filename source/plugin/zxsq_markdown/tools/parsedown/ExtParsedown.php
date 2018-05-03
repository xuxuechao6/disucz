<?php
/**
 * Usage:
 * File Name: ExtParsedown.php
 * Author: tecbbs
 * Mail: admin@tecbbs.com
 * Created Time: 2017-06-24 12:30:33
 **/

require_once(dirname(__FILE__) . '/' . 'Parsedown.php');

class ExtParseDown extends ParseDown {
	private $headingIdPre = "";
	public function setHeadingIdPre($pre)
	{
		$this->headingIdPre = $pre;
	}
	#
	# Fenced Code
	# language 去掉，直接用代码名称作为class

	public function checkSuperPlot() {
		global $_G;
		return array_key_exists('zxsq_superplot', $_G['cache']['plugin']);
	}

	protected function blockFencedCode($Line)
	{
		if ($this->checkSuperPlot()&&preg_match('/^['.$Line['text'][0].']{3,}[ ]*plot(:[\w:]+)?[ ]*([\w]+)?[ ]*$/', $Line['text'], $matches))
		{
			$plot = new plugin_zxsq_superplot();
			$id = $plot->getGUID();
			$cht = 'gv';
			$chof = 'png';
			if(isset($matches[1])) {
				$cht = substr($matches[1], 1);
			}
			if(isset($matches[2])) {
				$chof = $matches[2];
			}

			$Block = array(
				'isplot' => true,
				'cht' => $cht,
				'chof' => $chof,
				'char' => $Line['text'][0],
				'element' => array(
					'name' => 'div',
					'handler' => 'elements',
					'text' => array(),
					'attributes' => array(
						'class' => 'zxsq_superplot_form',
					),
				),
			);


			$Form = array();
			$Form [] = array(
				'name' => 'textarea',
				'text' => '',
				'attributes' => array(
					'name' => 'chl',
					'id' => 'chl_' . $id,
				),
			);
			$Form [] = array(
				'name' => 'input',
				'attributes' => array(
					'name' => 'cht',
					'id' => 'cht_' . $id,
					'value' => $cht,
				),
			);
			$Form [] = array(
				'name' => 'input',
				'attributes' => array(
					'name' => 'chof',
					'id' => 'chof_' . $id,
					'value' => $chof,
				),
			);
			$Block['element']['text'] [] = array(
				'name' => 'form',
				'text' => $Form,
				'handler' => 'elements',
				'attributes' => array(
					'style' => 'display:none',
					'id' => $id,
					'accept-charset' => 'utf-8',
					'name' => $id,
					'method' => 'post',
					'action' => $plot->setApi(),
					'enctype' => 'application/x-www-form-urlencoded',
				),
			);

			$Img = array(
				'name' => 'img',
				'attributes' => array(
					'id' => 'img_' . $id,
					'style' => 'max-width:90%;',
					'src' => '',
					'alt' => 'superplot',
					'title' => 'superplot',
					'onerror' => "this.onerror='';src='source/plugin/zxsq_superplot/images/loading.gif'", /*无效 开启安全模式后被sanitiseElement函数过滤 */
				),
			);
			$Block['element']['text'] [] = array(
				'name' => 'div',
				'text' => $Img,
				'handler' => 'element',
				'attributes' => array(
					'style' => 'text-align:center; margin:0 auto;',
					'id' => 'show_' . $id,
				),
			);
			return $Block;
		}
		
		if (preg_match('/^['.$Line['text'][0].']{3,}[ ]*([\w-#:]+)?[ ]*([\w]+)?[ ]*$/', $Line['text'], $matches))
		{
			$Element = array(
				'name' => 'code',
				'text' => '',
			);
			if (isset($matches[1]))
			{
				$class = $matches[1];
				$Element['attributes'] = array(
					'class' => $class,
				);
			}
			$Block = array(
				'char' => $Line['text'][0],
				'element' => array(
					'name' => 'pre',
					'handler' => 'element',
					'text' => $Element,
				),
			);
			return $Block;
		}
	}	

	protected function blockFencedCodeContinue($Line, $Block) {
		if (isset($Block['complete']))
		{
			return;
		}

		if (isset($Block['interrupted']))
		{
			if(isset($Block['isplot'])) {
				$Block['element']['text'][0]['text'][0]['text'] .= "\n";
			}
			else {
				$Block['element']['text']['text'] .= "\n";
			}

			unset($Block['interrupted']);
		}

		if (preg_match('/^'.$Block['char'].'{3,}[ ]*$/', $Line['text']))
		{
			if(isset($Block['isplot'])) {
				$Block['element']['text'][0]['text'][0]['text'] = substr($Block['element']['text'][0]['text'][0]['text'], 1);	
			} else {
				$Block['element']['text']['text'] = substr($Block['element']['text']['text'], 1);
			}

			$Block['complete'] = true;

			return $Block;
		}

		if(isset($Block['isplot'])) {
			$Block['element']['text'][0]['text'][0]['text'] .= "\n".$Line['body'];
		} else {
			$Block['element']['text']['text'] .= "\n".$Line['body'];
		}

		return $Block;
	}

	protected function blockFencedCodeComplete($Block)
	{
		if(isset($Block['isplot'])) {
			$text = $Block['element']['text'][0]['text'][0]['text'];
			$Block['element']['text'][0]['text'][0]['text'] = $text;
			$plot = new plugin_zxsq_superplot();
			$plot->setApi();
			$Block['element']['text'][1]['text']['attributes']['src'] = $plot->cal_file_name($Block['cht'], str_replace("\n", "\r\n", $text), $Block['chof']);
		}
		else {
			$text = $Block['element']['text']['text'];
			$Block['element']['text']['text'] = $text;
		}
		return $Block;
	}
	
	# 标题id空格特殊字符替换
	protected function slugify($text)
	{
		$slug = trim($text);
		$slug = strtr($slug, ' ', '-');
		$slug = strtolower($slug);
		return($slug);
	}
	protected function getHeadingId($text)
	{
		$slug = $this->slugify($text);
		$attributeId = $slug;

		if (!isset($this->headingSlugs[$slug])) {
			$this->headingSlugs[$slug] = 0;
		}

		if ($this->headingSlugs[$slug] > 0) {
			$attributeId .= '-'.$this->headingSlugs[$slug];
		}

		$this->headingSlugs[$slug]++;

		return $this->headingIdPre . "_" . $attributeId;
	}
	
 	#
	# Header
	protected function blockHeader($Line)
	{
		if (isset($Line['text'][1]))
		{
			$level = 1;
			while (isset($Line['text'][$level]) and $Line['text'][$level] === '#')
			{
				$level ++;
			}
			if ($level > 6)
			{
				return;
			}
			$text = trim($Line['text'], '# ');
			$Block = array(
				'element' => array(
					'name' => 'h' . min(6, $level),
					'text' => $text,
					'handler' => 'line',
					'attributes' => array(
						'id' => $this->getHeadingId($text),
					)
				),
			);
			return $Block;
		}
	}	
}
