<?php

class ChatViewModel extends ViewModel {
    public $viewFields = array(
        'Chat'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'User'=>array('user_id','user_type','real_name','wx_nickname','wx_sex','wx_headurl','_on'=>'Chat.user_id=User.user_id'),
    );
}
?>