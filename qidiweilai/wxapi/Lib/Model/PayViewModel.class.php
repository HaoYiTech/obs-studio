<?php

class PayViewModel extends ViewModel {
    public $viewFields = array(
        'Consume'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Subject'=>array('subject_id','subject_name','_on'=>'Subject.subject_id=Consume.subject_id','_type'=>'LEFT'),
        'Grade'=>array('grade_id','grade_name','_on'=>'Grade.grade_id=Consume.grade_id','_type'=>'LEFT'),
        'Shop'=>array('shop_id','name'=>'shop_name','_on'=>'Shop.shop_id=Consume.shop_id'),
    );
}
?>