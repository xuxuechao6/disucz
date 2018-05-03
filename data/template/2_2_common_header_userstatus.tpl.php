<?php if(!defined('IN_DISCUZ')) exit('Access Denied'); if($_G['uid']) { ?>
<div id="login">
    <ul>
            <li class="username">
                    <a href="home.php?mod=space&amp;uid=<?php echo $_G['uid'];?>" onmouseover="showMenu({'ctrlid':'user_con','pos':'34'});" class="user cl" id="user_con">
                            <span class="user_name"><?php echo $_G['member']['username'];?></span><?php echo avatar($_G[uid],small);?>                        </a>
               </li>
         <?php if($_G['group']['allowinvisible']) { ?>
        <li  class="loginstatus">
                <span id="loginstatus">
                        <a id="loginstatusid" href="member.php?mod=switchstatus" title="切换在线状态" onclick="ajaxget(this.href, 'loginstatus');return false;" class="xi2"></a>
                    </span>
        </li>
           <?php } ?>
         

           <!-- 不知道是啥 -->

           <!-- <li  id="hscbar" onmouseover="showMenu({'ctrlid':'hscbar','pos':'34!','ctrlclass':'a','duration':2});">
           </li> -->

        
        
            
            <li>
                    <span class="pipe">|</span><a href="home.php?mod=space&amp;do=pm" id="pm_ntc"<?php if($_G['member']['newpm']) { ?> class="new"<?php } ?>>消息</a>

            </li>
            <li>
                    <span class="pipe">|</span><a href="home.php?mod=space&amp;do=notice" id="myprompt" class="a showmenu<?php if($_G['member']['newprompt']) { ?> new<?php } ?>" onmouseover="showMenu({'ctrlid':'myprompt'});">提醒<?php if($_G['member']['newprompt']) { ?>(<?php echo $_G['member']['newprompt'];?>)<?php } ?></a><span id="myprompt_check"></span>

            </li>
<?php if(empty($_G['cookie']['ignore_notice']) && ($_G['member']['newpm'] || $_G['member']['newprompt_num']['follower'] || $_G['member']['newprompt_num']['follow'] || $_G['member']['newprompt'])) { ?><script language="javascript">delayShow($('myprompt'), function() {showMenu({'ctrlid':'myprompt','duration':3})});</script><?php } ?>
        <?php if($_G['setting']['taskon'] && !empty($_G['cookie']['taskdoing_'.$_G['uid']])) { ?>
        <!-- <li>
                <span class="pipe">|</span><a href="home.php?mod=task&amp;item=doing" id="task_ntc" class="new">进行中的任务</a>

        </li>   -->
        <?php } ?>
        
            <li>
                    <span class="pipe">|</span><a href="forum.php?mod=misc&amp;action=nav"  onclick="showWindow('nav', this.href, 'get', 0)">发帖</a>
            </li>


    

    </ul>
    

</div>
<?php } else { ?>

<div id="login">
        <a class="loginbtn" href="./member.php?mod=logging&amp;action=login">登录</a>
        <a href="./member.php?mod=register" class="registerbtn">注册</a>
    </div>

<?php } ?>

