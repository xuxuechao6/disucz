$postMarkdown = document.querySelector('.parsedown-markdown');
$postMarkdownStr = $postMarkdown.innerHTML;
$postMarkdownStr = $postMarkdownStr.replace(/<br>/ig, "");
$postMarkdown.innerHTML = $postMarkdownStr;
