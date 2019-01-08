<?php

class CommentViewModel extends ViewModel {
    public $viewFields = array(
        'Comment'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'User'=>array('user_id','wx_headurl','wx_nickname','_on'=>'User.user_id=Comment.user_id'),
    );
}
?>