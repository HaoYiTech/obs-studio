<?php

class FlowUserModel extends ViewModel {
    public $viewFields = array(
        'Flow'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'User'=>array('user_id','wx_nickname','_on'=>'Flow.user_id=User.user_id','_type'=>'LEFT'),
        'Room'=>array('room_id','agent_id','_on'=>'Flow.room_id=Room.room_id','_type'=>'LEFT'),
        'Agent'=>array('agent_id','cpm','_on'=>'Agent.agent_id=Room.agent_id'),
    );
}
?>