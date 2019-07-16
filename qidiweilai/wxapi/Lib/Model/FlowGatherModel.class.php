<?php

class FlowGatherModel extends ViewModel {
    public $viewFields = array(
        'Flow'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Gather'=>array('gather_id','name_pc','_on'=>'Flow.gather_id=Gather.gather_id'),
    );
}
?>